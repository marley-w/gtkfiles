#include <gtk/gtk.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <string>
#include <sstream>

namespace fs = std::filesystem;

// Helper function to run shell commands and capture output
std::string popen_output(const std::string& command) {
    char buffer[128];
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);
    return result;
}

// Function to get appropriate icon for file or folder based on MIME type
GdkPixbuf* get_icon_for_item(const std::string& path, bool is_folder) {
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    GError* error = NULL;

    if (is_folder) {
        return gtk_icon_theme_load_icon(icon_theme, "folder", 24, GTK_ICON_LOOKUP_USE_BUILTIN, &error);
    }

    GFile* file = g_file_new_for_path(path.c_str());
    GFileInfo* info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, &error);
    if (error) {
        g_error_free(error);
        g_object_unref(file);
        return nullptr;
    }

    const char* mime_type = g_file_info_get_content_type(info);
    GtkIconInfo* icon_info = gtk_icon_theme_lookup_by_gicon(icon_theme, g_content_type_get_icon(mime_type), 24, GTK_ICON_LOOKUP_USE_BUILTIN);

    g_object_unref(info);
    g_object_unref(file);

    if (!icon_info) {
        return nullptr;
    }

    GdkPixbuf* pixbuf = gtk_icon_info_load_icon(icon_info, &error);
    g_object_unref(icon_info);

    if (error) {
        g_error_free(error);
        return nullptr;
    }

    return pixbuf;
}

// Function to get available applications for a file (via `xdg-mime` and `gio`)
std::vector<std::string> get_available_apps(const std::string& file_path) {
    std::vector<std::string> apps;

    // Get MIME type of the file
    std::ostringstream mime_type_cmd;
    mime_type_cmd << "xdg-mime query filetype " << file_path;
    std::string mime_type = popen_output(mime_type_cmd.str());

    if (!mime_type.empty()) {
        // Use `gio` to list applications for the MIME type
        std::ostringstream apps_cmd;
        apps_cmd << "gio mime " << mime_type;

        std::string output = popen_output(apps_cmd.str());
        std::istringstream stream(output);

        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line[0] != ' ') { // Skip indent lines
                apps.push_back(line);
            }
        }
    }

    return apps;
}

// Function to open a file with a specified application
void open_with(const std::string& app, const std::string& file_path) {
    std::ostringstream cmd;
    cmd << app << " \"" << file_path << "\"";
    g_spawn_command_line_async(cmd.str().c_str(), NULL);
}

// Function to delete a file or folder
void delete_item(const std::string& file_path) {
    try {
        if (fs::is_directory(file_path)) {
            fs::remove_all(file_path); // Recursively delete folder
        } else {
            fs::remove(file_path); // Delete file
        }
    } catch (const std::exception& e) {
        std::cerr << "Error deleting " << file_path << ": " << e.what() << std::endl;
    }
}

// Function to populate the ListStore with directory contents
void populate_file_list(GtkListStore* list_store, const std::string& path) {
    gtk_list_store_clear(list_store);

    for (const auto& entry : fs::directory_iterator(path)) {
        GtkTreeIter iter;
        std::string name = entry.path().filename().string();
        std::string full_path = entry.path().string();
        bool is_folder = entry.is_directory();

        GdkPixbuf* icon = get_icon_for_item(full_path, is_folder);

        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                           0, icon, // Set the icon
                           1, name.c_str(),
                           2, full_path.c_str(),
                           3, is_folder ? "Folder" : "File",
                           -1);

        if (icon) {
            g_object_unref(icon); // Free the icon once added
        }
    }
}

// Callback for "Open With" submenu
void on_open_with(GtkWidget* menu_item, gpointer data) {
    std::pair<std::string, std::string>* file_app_pair = static_cast<std::pair<std::string, std::string>*>(data);
    const std::string& app = file_app_pair->first;
    const std::string& file_path = file_app_pair->second;
    open_with(app, file_path);
    delete file_app_pair; // Clean up dynamic allocation
}

// Callback for "Delete" option
void on_delete(GtkWidget* menu_item, gpointer user_data) {
    std::string* file_path = static_cast<std::string*>(user_data);

    // Confirmation dialog
    GtkWidget* dialog = gtk_message_dialog_new(NULL,
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_YES_NO,
                                               "Are you sure you want to delete '%s'?", file_path->c_str());

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
        delete_item(*file_path);
    }

    gtk_widget_destroy(dialog);
    delete file_path; // Clean up dynamic allocation
}

// Create a right-click menu for files/folders
gboolean on_tree_view_button_press(GtkWidget* tree_view, GdkEventButton* event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) { // Right-click
        GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
        GtkTreeModel* model;
        GtkTreeIter iter;

        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            gchar* name;
            gchar* path;

            gtk_tree_model_get(model, &iter, 1, &name, 2, &path, -1);

            if (name && path) {
                GtkWidget* menu = gtk_menu_new();

                // "Open With" submenu
                GtkWidget* open_with_menu = gtk_menu_item_new_with_label("Open With");
                GtkWidget* open_with_submenu = gtk_menu_new();
                std::vector<std::string> apps = get_available_apps(path);

                if (!apps.empty()) {
                    for (const auto& app : apps) {
                        GtkWidget* app_item = gtk_menu_item_new_with_label(app.c_str());
                        auto* app_data = new std::pair<std::string, std::string>(app, path);
                        g_signal_connect(app_item, "activate", G_CALLBACK(on_open_with), app_data);
                        gtk_menu_shell_append(GTK_MENU_SHELL(open_with_submenu), app_item);
                    }
                } else {
                    GtkWidget* no_apps_item = gtk_menu_item_new_with_label("No applications found");
                    gtk_widget_set_sensitive(no_apps_item, FALSE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(open_with_submenu), no_apps_item);
                }

                gtk_menu_item_set_submenu(GTK_MENU_ITEM(open_with_menu), open_with_submenu);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), open_with_menu);

                // "Delete" option
                GtkWidget* delete_item = gtk_menu_item_new_with_label("Delete");
                auto* path_copy = new std::string(path);
                g_signal_connect(delete_item, "activate", G_CALLBACK(on_delete), path_copy);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);

                gtk_widget_show_all(menu);
                gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);

                g_free(name);
                g_free(path);
            }
        }
        return TRUE; // Event handled
    }
    return FALSE; // Event not handled
}

// Callback for search bar navigation
void on_search_enter(GtkEntry* search_entry, gpointer user_data) {
    GtkListStore* list_store = GTK_LIST_STORE(g_object_get_data(G_OBJECT(search_entry), "list_store"));
    gchar** current_path = (gchar**)g_object_get_data(G_OBJECT(search_entry), "current_path");

    const char* entered_path = gtk_entry_get_text(search_entry);

    // Validate and navigate to the entered path
    if (entered_path && fs::exists(entered_path) && fs::is_directory(entered_path)) {
        g_free(*current_path);
        *current_path = g_strdup(entered_path);

        populate_file_list(list_store, entered_path);
    } else {
        GtkWidget* error_dialog = gtk_message_dialog_new(NULL,
                                                         GTK_DIALOG_MODAL,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "The entered path does not exist or is not a directory.");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
    }
}

// Callback for the Back button
void on_back_button_clicked(GtkWidget* button, gpointer user_data) {
    GtkListStore* list_store = GTK_LIST_STORE(g_object_get_data(G_OBJECT(button), "list_store"));
    gchar** current_path = (gchar**)g_object_get_data(G_OBJECT(button), "current_path");

    // Determine the parent directory
    fs::path parent_path = fs::path(*current_path).parent_path();

    if (!parent_path.empty() && fs::exists(parent_path)) {
        g_free(*current_path);
        *current_path = g_strdup(parent_path.c_str());
        populate_file_list(list_store, *current_path);
    } else {
        GtkWidget* error_dialog = gtk_message_dialog_new(NULL,
                                                         GTK_DIALOG_MODAL,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "No parent directory exists.");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
    }
}

// Callback for directory chooser
void on_dir_selected(GtkWidget* file_chooser, gpointer data) {
    GtkListStore* list_store = GTK_LIST_STORE(g_object_get_data(G_OBJECT(file_chooser), "list_store"));
    gchar** current_path = (gchar**)g_object_get_data(G_OBJECT(file_chooser), "current_path");

    char* folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
    if (folder) {
        g_free(*current_path);
        *current_path = g_strdup(folder);

        populate_file_list(list_store, folder);
        g_free(folder);
    }
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "GTK Files");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Horizontal box for Back button, directory chooser, and search bar
    GtkWidget* hbox_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_top, FALSE, FALSE, 0);

    // Back button
    GtkWidget* back_button = gtk_button_new_with_label("Back");
    gtk_box_pack_start(GTK_BOX(hbox_top), back_button, FALSE, FALSE, 0);

    // Directory chooser
    GtkWidget* file_chooser = gtk_file_chooser_button_new("Select Directory", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_widget_set_size_request(file_chooser, 200, -1); // Make it smaller horizontally
    gtk_box_pack_start(GTK_BOX(hbox_top), file_chooser, FALSE, FALSE, 0);

    // Search bar
    GtkWidget* search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Enter path to navigate...");
    gtk_box_pack_start(GTK_BOX(hbox_top), search_entry, TRUE, TRUE, 0);

    // TreeView for displaying files
    GtkListStore* list_store = gtk_list_store_new(4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    g_object_unref(list_store);

    GtkCellRenderer* renderer_icon = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn* col_icon = gtk_tree_view_column_new_with_attributes("Icon", renderer_icon, "pixbuf", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_icon);

    GtkCellRendererText* renderer_text = GTK_CELL_RENDERER_TEXT(gtk_cell_renderer_text_new());
    GtkTreeViewColumn* col_name = gtk_tree_view_column_new_with_attributes("Name", GTK_CELL_RENDERER(renderer_text), "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_name);

    GtkTreeViewColumn* col_type = gtk_tree_view_column_new_with_attributes("Type", gtk_cell_renderer_text_new(), "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_type);

    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    gchar* current_path = g_strdup(getenv("HOME"));
    g_object_set_data(G_OBJECT(back_button), "current_path", &current_path);
    g_object_set_data(G_OBJECT(file_chooser), "current_path", &current_path);
    g_object_set_data(G_OBJECT(search_entry), "current_path", &current_path);
    g_object_set_data(G_OBJECT(back_button), "list_store", list_store);
    g_object_set_data(G_OBJECT(search_entry), "list_store", list_store);
    g_object_set_data(G_OBJECT(file_chooser), "list_store", list_store);

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), current_path);
    populate_file_list(list_store, current_path);

    // Signal connections
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(file_chooser, "selection-changed", G_CALLBACK(on_dir_selected), file_chooser);
    g_signal_connect(search_entry, "activate", G_CALLBACK(on_search_enter), NULL);
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_button_clicked), NULL);
    g_signal_connect(tree_view, "button-press-event", G_CALLBACK(on_tree_view_button_press), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    g_free(current_path);
    return 0;
}

