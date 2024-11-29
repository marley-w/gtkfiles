#include <gtk/gtk.h>
#include <iostream>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

// Function to get appropriate icon for file or folder based on MIME type
GdkPixbuf* get_icon_for_item(const std::string& path, bool is_folder) {
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    GError* error = NULL;

    // If it's a folder, always use the "folder" icon
    if (is_folder) {
        return gtk_icon_theme_load_icon(icon_theme, "folder", 24, GTK_ICON_LOOKUP_USE_BUILTIN, &error);
    }

    // Get MIME type for the file
    GFile* file = g_file_new_for_path(path.c_str());
    GFileInfo* info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, &error);
    if (error) {
        g_error_free(error);
        g_object_unref(file);
        return nullptr; // Fallback if MIME type can't be retrieved
    }

    const char* mime_type = g_file_info_get_content_type(info);
    GtkIconInfo* icon_info = gtk_icon_theme_lookup_by_gicon(icon_theme, g_content_type_get_icon(mime_type), 24, GTK_ICON_LOOKUP_USE_BUILTIN);

    g_object_unref(info);
    g_object_unref(file);

    if (!icon_info) {
        return nullptr; // No icon found
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
    gtk_list_store_clear(list_store); // Clear existing entries

    for (const auto& entry : fs::directory_iterator(path)) {
        GtkTreeIter iter;
        std::string name = entry.path().filename().string();
        std::string full_path = entry.path().string();
        bool is_folder = entry.is_directory();

        GdkPixbuf* icon = get_icon_for_item(full_path, is_folder); // Get icon for each item

        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                           0, icon,           // Icon
                           1, name.c_str(),   // File/Folder name
                           2, is_folder ? "Folder" : "File", // Type
                           -1);

        if (icon) {
            g_object_unref(icon); // Free the icon once added to the store
        }
    }
}

// Callback for directory selection
void on_dir_selected(GtkWidget* file_chooser, gpointer data) {
    GtkListStore* list_store = GTK_LIST_STORE(g_object_get_data(G_OBJECT(file_chooser), "list_store"));
    gchar** current_path = (gchar**)g_object_get_data(G_OBJECT(file_chooser), "current_path");

    char* folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
    if (folder) {
        g_free(*current_path); // Free the old path
        *current_path = g_strdup(folder);

        populate_file_list(list_store, folder);
        g_free(folder); // Free the folder pointer
    }
}

// Callback for row activation (double-click or Enter key)
void on_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data) {
    GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar* name;
        gchar* type;
        gtk_tree_model_get(model, &iter, 1, &name, 2, &type, -1);

        if (g_strcmp0(type, "Folder") == 0) { // Navigate into folder
            gchar** current_path = (gchar**)user_data;
            std::string new_path = std::string(*current_path) + "/" + name;

            // Check if the path is valid and accessible
            if (fs::exists(new_path) && fs::is_directory(new_path)) {
                GtkListStore* list_store = GTK_LIST_STORE(model);

                g_free(*current_path);
                *current_path = g_strdup(new_path.c_str());

                populate_file_list(list_store, new_path);
            } else {
                std::cerr << "Error: Cannot open folder: " << new_path << std::endl;
            }
        }

        g_free(name);
        g_free(type);
    }
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    // Main window
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "GTK Files");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Vertical box for layout
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // File chooser button
    GtkWidget* file_chooser = gtk_file_chooser_button_new("Select Directory", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_box_pack_start(GTK_BOX(vbox), file_chooser, FALSE, FALSE, 0);

    // TreeView for displaying files
    GtkListStore* list_store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    g_object_unref(list_store); // The tree view now owns the model

    // Columns for TreeView
    GtkCellRenderer* renderer_icon = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn* col_icon = gtk_tree_view_column_new_with_attributes("Icon", renderer_icon, "pixbuf", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_icon);

    GtkCellRenderer* renderer_text = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* col_name = gtk_tree_view_column_new_with_attributes("Name", renderer_text, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_name);

    GtkTreeViewColumn* col_type = gtk_tree_view_column_new_with_attributes("Type", renderer_text, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_type);

    // Add TreeView to a scrolled window
    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    // Current path tracker
    gchar* current_path = g_strdup(fs::current_path().c_str());
    g_object_set_data(G_OBJECT(file_chooser), "current_path", &current_path);

    // Populate the initial directory listing
    populate_file_list(list_store, current_path);

    // Connect signals
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(file_chooser, "selection-changed", G_CALLBACK(on_dir_selected), file_chooser);
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_row_activated), &current_path);

    // Pass the ListStore to the file chooser callback
    g_object_set_data(G_OBJECT(file_chooser), "list_store", list_store);

    // Show all widgets
    gtk_widget_show_all(window);

    // Run the GTK main loop
    gtk_main();

    // Cleanup
    g_free(current_path);

    return 0;
}

