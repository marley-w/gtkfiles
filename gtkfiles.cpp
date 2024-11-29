#include <gtk/gtk.h>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

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
                           0, icon,
                           1, name.c_str(),
                           2, full_path.c_str(),
                           3, is_folder ? "Folder" : "File",
                           -1);

        if (icon) {
            g_object_unref(icon);
        }
    }
}

// Function to display error dialog
void show_error_dialog(GtkWidget* parent, const char* message) {
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
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
        show_error_dialog(NULL, "The entered path does not exist or is not a directory.");
    }
}

// Callback for directory selection
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

    // Horizontal box for directory chooser and search bar
    GtkWidget* hbox_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_top, FALSE, FALSE, 0);

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

    gchar* current_path = g_strdup(fs::current_path().c_str());
    g_object_set_data(G_OBJECT(file_chooser), "current_path", &current_path);
    g_object_set_data(G_OBJECT(search_entry), "current_path", &current_path);
    g_object_set_data(G_OBJECT(search_entry), "list_store", list_store);

    populate_file_list(list_store, current_path);

    // Signal connections
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(file_chooser, "selection-changed", G_CALLBACK(on_dir_selected), file_chooser);
    g_signal_connect(search_entry, "activate", G_CALLBACK(on_search_enter), NULL);

    g_object_set_data(G_OBJECT(file_chooser), "list_store", list_store);

    gtk_widget_show_all(window);
    gtk_main();

    g_free(current_path);
    return 0;
}

