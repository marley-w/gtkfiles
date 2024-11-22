// File: HyprFiles.cpp
#include <gtk/gtk.h>
#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <filesystem>

namespace fs = std::filesystem;

// Global variables
GtkWidget *window;
GtkWidget *file_list;
GtkWidget *path_entry;
std::stack<std::string> directory_history;
std::string current_directory;

// Function to update the path entry
void update_path_entry() {
    gtk_entry_set_text(GTK_ENTRY(path_entry), current_directory.c_str());
}

// Function to populate the file list
void populate_file_list(const std::string &path) {
    gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(file_list))));
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(file_list)));
    GtkTreeIter iter;
    GdkPixbuf *icon;
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();

    try {
        current_directory = path;
        update_path_entry();

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
        std::cerr << "Error accessing directory: " << e.what() << std::endl;
    }
}

// Callback for row activation (double-click)
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

// Callback for the Back button
void on_back_clicked(GtkButton *button, gpointer user_data) {
    if (!directory_history.empty()) {
        std::string previous_directory = directory_history.top();
        directory_history.pop();
        populate_file_list(previous_directory);
    }
}

// Callback for the Home (`~/`) button
void on_home_clicked(GtkButton *button, gpointer user_data) {
    directory_history.push(current_directory);
    populate_file_list(getenv("HOME"));
}

// Callback for the Root (`/`) button
void on_root_clicked(GtkButton *button, gpointer user_data) {
    directory_history.push(current_directory);
    populate_file_list("/");
}

// Callback for the search bar activation
void on_path_entry_activate(GtkEntry *entry, gpointer user_data) {
    const gchar *input_path = gtk_entry_get_text(entry);
    std::string new_path = input_path;

    if (fs::exists(new_path) && fs::is_directory(new_path)) {
        directory_history.push(current_directory);
        populate_file_list(new_path);
    } else {
        std::cerr << "Invalid directory: " << new_path << std::endl;
    }
}

// Main function
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Window setup
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "HyprFiles");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Vertical box for layout
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Horizontal box for buttons and search bar
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    // Back button
    GtkWidget *back_button = gtk_button_new_with_label("Back");
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), back_button, FALSE, FALSE, 0);

    // Home (`~/`) button
    GtkWidget *home_button = gtk_button_new_with_label("~");
    g_signal_connect(home_button, "clicked", G_CALLBACK(on_home_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), home_button, FALSE, FALSE, 0);

    // Root (`/`) button
    GtkWidget *root_button = gtk_button_new_with_label("/");
    g_signal_connect(root_button, "clicked", G_CALLBACK(on_root_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), root_button, FALSE, FALSE, 0);

    // Path entry (search bar)
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

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), file_list);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    // Load initial directory
    current_directory = getenv("HOME");
    populate_file_list(current_directory);

    // Show all widgets
    gtk_widget_show_all(window);

    // Run GTK main loop
    gtk_main();

    return 0;
}

