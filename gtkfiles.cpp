#include <gtk/gtk.h>
#include <iostream>
#include <string>
#include <stack>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

// Global variables
GtkWidget *window;
GtkWidget *file_list;
GtkWidget *path_entry;
std::stack<std::string> directory_history;
std::string current_directory;
std::string selected_file;

// Function declarations
void populate_file_list(const std::string &path);
void on_back_clicked(GtkButton *button, gpointer user_data);
void on_home_clicked(GtkButton *button, gpointer user_data);
void on_root_clicked(GtkButton *button, gpointer user_data);
void on_path_entry_activate(GtkEntry *entry, gpointer user_data);
void on_delete_clicked(GtkMenuItem *menu_item, gpointer user_data);
void on_rename_clicked(GtkMenuItem *menu_item, gpointer user_data);
void on_open_with_clicked(GtkMenuItem *menu_item, gpointer user_data);
gboolean on_tree_view_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
void show_error_dialog(const std::string &error_message);
void open_with_app_clicked(GtkWidget *button, gpointer user_data);

// Error dialog
void show_error_dialog(const std::string &error_message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_OK,
                                               "Error: %s", error_message.c_str());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Populate the file list
void populate_file_list(const std::string &path) {
    gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(file_list))));
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(file_list)));
    GtkTreeIter iter;
    GdkPixbuf *icon;
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();

    try {
        current_directory = path;
        gtk_entry_set_text(GTK_ENTRY(path_entry), current_directory.c_str());

        for (const auto &entry : fs::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            std::string icon_name = entry.is_directory() ? "folder" : "text-x-generic";

            icon = gtk_icon_theme_load_icon(icon_theme, icon_name.c_str(), 48, GTK_ICON_LOOKUP_USE_BUILTIN, nullptr);
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               0, icon,
                               1, name.c_str(),
                               -1);
            if (icon) g_object_unref(icon);
        }
    } catch (const fs::filesystem_error &e) {
        show_error_dialog(e.what());
    }
}

// Row activation (double-click)
void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *file_name;

    model = gtk_tree_view_get_model(tree_view);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter, 1, &file_name, -1);
        std::string new_path = current_directory + "/" + file_name;

        if (fs::is_directory(new_path)) {
            directory_history.push(current_directory);
            populate_file_list(new_path);
        } else {
            std::cout << "File selected: " << new_path << std::endl;
        }
        g_free(file_name);
    }
}

// Back button
void on_back_clicked(GtkButton *button, gpointer user_data) {
    if (!directory_history.empty()) {
        std::string previous_directory = directory_history.top();
        directory_history.pop();
        populate_file_list(previous_directory);
    }
}

// Home button
void on_home_clicked(GtkButton *button, gpointer user_data) {
    directory_history.push(current_directory);
    populate_file_list(getenv("HOME"));
}

// Root button
void on_root_clicked(GtkButton *button, gpointer user_data) {
    directory_history.push(current_directory);
    populate_file_list("/");
}

// Path entry activated
void on_path_entry_activate(GtkEntry *entry, gpointer user_data) {
    const gchar *input_path = gtk_entry_get_text(entry);
    std::string new_path = input_path;

    if (fs::exists(new_path) && fs::is_directory(new_path)) {
        directory_history.push(current_directory);
        populate_file_list(new_path);
    } else {
        show_error_dialog("Invalid directory path.");
    }
}

// Delete file
void on_delete_clicked(GtkMenuItem *menu_item, gpointer user_data) {
    if (selected_file.empty()) {
        show_error_dialog("No file selected for deletion.");
        return;
    }

    try {
        fs::remove_all(selected_file);
        populate_file_list(current_directory);
    } catch (const fs::filesystem_error &e) {
        show_error_dialog(std::string("Failed to delete file: ") + e.what());
    }
}

// Rename file
void on_rename_clicked(GtkMenuItem *menu_item, gpointer user_data) {
    if (selected_file.empty()) {
        show_error_dialog("No file selected for renaming.");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Rename",
                                                    GTK_WINDOW(window),
                                                    GTK_DIALOG_MODAL,
                                                    "_OK", GTK_RESPONSE_OK,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), fs::path(selected_file).filename().c_str());
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 5);
    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        const gchar *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
        try {
            std::string new_path = fs::path(selected_file).parent_path() / new_name;
            fs::rename(selected_file, new_path);
            populate_file_list(current_directory);
        } catch (const fs::filesystem_error &e) {
            show_error_dialog(std::string("Failed to rename: ") + e.what());
        }
    }
    gtk_widget_destroy(dialog);
}

// Open with apps
void on_open_with_clicked(GtkMenuItem *menu_item, gpointer user_data) {
    if (selected_file.empty()) {
        show_error_dialog("No file selected for opening.");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Open With",
                                                    GTK_WINDOW(window),
                                                    GTK_DIALOG_MODAL,
                                                    "_Close", GTK_RESPONSE_CLOSE,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *listbox = gtk_list_box_new();
    gtk_box_pack_start(GTK_BOX(content_area), listbox, TRUE, TRUE, 5);

    std::vector<std::string> allowed_apps = {"kitty", "wine", "nvim", "zen-browser", "file-roller"};

    for (const auto &app_name : allowed_apps) {
        GtkWidget *button = gtk_button_new_with_label(app_name.c_str());
        gtk_list_box_insert(GTK_LIST_BOX(listbox), button, -1);

        g_signal_connect(button, "clicked", G_CALLBACK(open_with_app_clicked), dialog);
    }

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Open file with custom commands
void open_with_app_clicked(GtkWidget *button, gpointer user_data) {
    const gchar *app_name = gtk_button_get_label(GTK_BUTTON(button));

    std::string command;
    if (std::string(app_name) == "nvim") {
        command = "kitty --hold nvim \"" + selected_file + "\"";
    } else {
        command = std::string(app_name) + " \"" + selected_file + "\"";
    }

    if (system(command.c_str()) != 0) {
        show_error_dialog("Failed to launch application.");
    }

    GtkWidget *dialog = GTK_WIDGET(user_data);
    gtk_widget_destroy(dialog);
}

// Right-click menu
gboolean on_tree_view_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) { // Right-click
        GtkTreeView *tree_view = GTK_TREE_VIEW(widget);
        GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
        GtkTreePath *path;
        GtkTreeIter iter;

        if (gtk_tree_view_get_path_at_pos(tree_view, event->x, event->y, &path, NULL, NULL, NULL)) {
            gtk_tree_model_get_iter(model, &iter, path);
            gchar *file_name;
            gtk_tree_model_get(model, &iter, 1, &file_name, -1);

            selected_file = current_directory + "/" + file_name;

            GtkWidget *menu = gtk_menu_new();
            GtkWidget *delete_item = gtk_menu_item_new_with_label("Delete");
            GtkWidget *rename_item = gtk_menu_item_new_with_label("Rename");
            GtkWidget *open_with_item = gtk_menu_item_new_with_label("Open With");

            g_signal_connect(delete_item, "activate", G_CALLBACK(on_delete_clicked), NULL);
            g_signal_connect(rename_item, "activate", G_CALLBACK(on_rename_clicked), NULL);
            g_signal_connect(open_with_item, "activate", G_CALLBACK(on_open_with_clicked), NULL);

            gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), rename_item);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), open_with_item);

            gtk_widget_show_all(menu);
            gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);

            g_free(file_name);
            gtk_tree_path_free(path);
            return TRUE;
        }
    }
    return FALSE;
}

// Main function
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Window setup
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "gtkfiles");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Layout setup
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *back_button = gtk_button_new_with_label("Back");
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), back_button, FALSE, FALSE, 0);

    GtkWidget *home_button = gtk_button_new_with_label("~");
    g_signal_connect(home_button, "clicked", G_CALLBACK(on_home_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), home_button, FALSE, FALSE, 0);

    GtkWidget *root_button = gtk_button_new_with_label("/");
    g_signal_connect(root_button, "clicked", G_CALLBACK(on_root_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), root_button, FALSE, FALSE, 0);

    path_entry = gtk_entry_new();
    g_signal_connect(path_entry, "activate", G_CALLBACK(on_path_entry_activate), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), path_entry, TRUE, TRUE, 0);

    // File list setup
    GtkListStore *store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
    file_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Icon", renderer, "pixbuf", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(file_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("File Name", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(file_list), column);

    g_signal_connect(file_list, "row-activated", G_CALLBACK(on_row_activated), NULL);
    g_signal_connect(file_list, "button-press-event", G_CALLBACK(on_tree_view_button_press), NULL);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), file_list);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    current_directory = getenv("HOME");
    populate_file_list(current_directory);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
