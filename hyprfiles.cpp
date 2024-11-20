#include <gtkmm.h>
#include <filesystem>
#include <iostream>
#include <stack>

namespace fs = std::filesystem;

class HyprFilesWindow : public Gtk::Window {
public:
    HyprFilesWindow();

private:
    Gtk::Box main_box;          // Main vertical box
    Gtk::Box button_box;        // Box for navigation buttons
    Gtk::TreeView file_view;    // File view
    Gtk::ScrolledWindow file_scroll;
    Glib::RefPtr<Gtk::ListStore> file_list_store;

    std::string current_directory;
    std::stack<std::string> back_stack;
    std::stack<std::string> forward_stack;

    // File View Columns
    class FileColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        FileColumns() {
            add(col_icon);
            add(col_name);
            add(col_is_dir);
        }

        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> col_icon;
        Gtk::TreeModelColumn<std::string> col_name;
        Gtk::TreeModelColumn<bool> col_is_dir;
    };

    FileColumns file_columns;

    // Navigation Buttons
    Gtk::Button button_home;
    Gtk::Button button_root;
    Gtk::Button button_back;
    Gtk::Button button_forward;

    // Path Editing
    Gtk::Button button_path;
    Gtk::Entry entry_path;
    Gtk::Box path_box;

    void load_directory(const std::string& path);
    void update_file_view();
    void on_file_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
    void navigate_home();
    void navigate_root();
    void navigate_back();
    void navigate_forward();
    void on_path_button_clicked();
    void on_path_entry_activate();

    // Icon Helper
    Glib::RefPtr<Gdk::Pixbuf> get_icon_for_file(const fs::path& filepath, bool is_dir);
};

HyprFilesWindow::HyprFilesWindow()
    : main_box(Gtk::ORIENTATION_VERTICAL, 5),
      button_box(Gtk::ORIENTATION_HORIZONTAL, 5),
      path_box(Gtk::ORIENTATION_HORIZONTAL, 5),
      current_directory(fs::current_path()),
      button_home("Home"),
      button_root("Root"),
      button_back("Back"),
      button_forward("Forward"),
      button_path("Path") {
    set_title("HyprFiles - File Manager");
    set_default_size(800, 600);

    // Add main layout
    add(main_box);

    // Add buttons to button box
    button_box.pack_start(button_back, Gtk::PACK_SHRINK);
    button_box.pack_start(button_forward, Gtk::PACK_SHRINK);
    button_box.pack_start(button_home, Gtk::PACK_SHRINK);
    button_box.pack_start(button_root, Gtk::PACK_SHRINK);
    button_box.pack_start(button_path, Gtk::PACK_SHRINK);
    main_box.pack_start(button_box, Gtk::PACK_SHRINK);

    // Connect button signals
    button_home.signal_clicked().connect(sigc::mem_fun(*this, &HyprFilesWindow::navigate_home));
    button_root.signal_clicked().connect(sigc::mem_fun(*this, &HyprFilesWindow::navigate_root));
    button_back.signal_clicked().connect(sigc::mem_fun(*this, &HyprFilesWindow::navigate_back));
    button_forward.signal_clicked().connect(sigc::mem_fun(*this, &HyprFilesWindow::navigate_forward));
    button_path.signal_clicked().connect(sigc::mem_fun(*this, &HyprFilesWindow::on_path_button_clicked));

    // Setup path entry box
    entry_path.set_placeholder_text("Enter path or search...");
    entry_path.signal_activate().connect(sigc::mem_fun(*this, &HyprFilesWindow::on_path_entry_activate));
    entry_path.hide(); // Initially hidden
    path_box.pack_start(entry_path, Gtk::PACK_EXPAND_WIDGET);
    button_box.pack_end(path_box, Gtk::PACK_EXPAND_WIDGET);

    // Setup file view
    file_list_store = Gtk::ListStore::create(file_columns);
    file_view.set_model(file_list_store);
    file_view.append_column("Icon", file_columns.col_icon);
    file_view.append_column("Name", file_columns.col_name);

    file_scroll.add(file_view);
    file_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    main_box.pack_start(file_scroll, Gtk::PACK_EXPAND_WIDGET);

    // Connect double-click signal
    file_view.signal_row_activated().connect(sigc::mem_fun(*this, &HyprFilesWindow::on_file_row_activated));

    // Load initial directory
    load_directory(current_directory);

    show_all_children();
}

void HyprFilesWindow::load_directory(const std::string& path) {
    if (fs::exists(path) && fs::is_directory(path)) {
        // Update history
        if (!current_directory.empty() && current_directory != path) {
            back_stack.push(current_directory);
            forward_stack = std::stack<std::string>(); // Clear forward history
        }

        current_directory = path;
        update_file_view();

        // Update path button text
        button_path.set_label(current_directory);
    } else {
        std::cerr << "Invalid path: " << path << std::endl;
    }
}

void HyprFilesWindow::update_file_view() {
    file_list_store->clear();

    try {
        for (const auto& entry : fs::directory_iterator(current_directory)) {
            auto row = *(file_list_store->append());
            row[file_columns.col_name] = entry.path().filename().string();
            row[file_columns.col_is_dir] = entry.is_directory();
            row[file_columns.col_icon] = get_icon_for_file(entry.path(), entry.is_directory());
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading directory: " << e.what() << std::endl;
    }
}

void HyprFilesWindow::on_file_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column) {
    Gtk::TreeModel::iterator iter = file_list_store->get_iter(path);
    if (!iter)
        return;

    Gtk::TreeModel::Row row = *iter;
    std::string name = row[file_columns.col_name];
    bool is_dir = row[file_columns.col_is_dir];

    if (is_dir) {
        load_directory(current_directory + "/" + name);
    }
}

void HyprFilesWindow::navigate_home() {
    load_directory(fs::path(getenv("HOME")).string());
}

void HyprFilesWindow::navigate_root() {
    load_directory("/");
}

void HyprFilesWindow::navigate_back() {
    if (back_stack.empty()) {
        std::cerr << "No previous directory to navigate back to." << std::endl;
        return;
    }

    forward_stack.push(current_directory); // Save current directory to forward stack
    std::string previous_directory = back_stack.top();
    back_stack.pop();

    load_directory(previous_directory);
}

void HyprFilesWindow::navigate_forward() {
    if (forward_stack.empty()) {
        std::cerr << "No forward directory to navigate to." << std::endl;
        return;
    }

    back_stack.push(current_directory); // Save current directory to back stack
    std::string next_directory = forward_stack.top();
    forward_stack.pop();

    load_directory(next_directory);
}

void HyprFilesWindow::on_path_button_clicked() {
    // Toggle visibility between button and entry
    if (button_path.get_visible()) {
        button_path.hide();
        entry_path.set_text(current_directory);
        entry_path.show();
    }
}

void HyprFilesWindow::on_path_entry_activate() {
    std::string input_path = entry_path.get_text();
    entry_path.hide();
    button_path.show();

    // Try loading the entered path or searching
    load_directory(input_path);
}

Glib::RefPtr<Gdk::Pixbuf> HyprFilesWindow::get_icon_for_file(const fs::path& filepath, bool is_dir) {
    Glib::RefPtr<Gtk::IconTheme> icon_theme = Gtk::IconTheme::get_default();

    try {
        if (is_dir) {
            return icon_theme->load_icon("folder", 24, Gtk::ICON_LOOKUP_USE_BUILTIN);
        } else {
            // Map extensions to icons
            std::string extension = filepath.extension().string();
            if (extension == ".txt") {
                return icon_theme->load_icon("text-x-generic", 24, Gtk::ICON_LOOKUP_USE_BUILTIN);
            } else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
                return icon_theme->load_icon("image-x-generic", 24, Gtk::ICON_LOOKUP_USE_BUILTIN);
            } else if (extension == ".pdf") {
                return icon_theme->load_icon("application-pdf", 24, Gtk::ICON_LOOKUP_USE_BUILTIN);
            } else {
                return icon_theme->load_icon("text-x-generic", 24, Gtk::ICON_LOOKUP_USE_BUILTIN);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading icon: " << e.what() << std::endl;
        return {};
    }
}

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "org.hyprfiles.filemanager");
    HyprFilesWindow window;
    return app->run(window);
}

