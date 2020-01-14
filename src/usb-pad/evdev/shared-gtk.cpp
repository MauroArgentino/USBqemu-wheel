#include "shared.h"
#include "../../osdebugout.h"
#include "icon_buzz_24.h"

#include <chrono>
#include <thread>
#include <stdio.h>
#include <sstream>

namespace usb_pad { namespace evdev {

using sys_clock = std::chrono::system_clock;
using ms = std::chrono::milliseconds;

#define JOYTYPE "joytype"
#define CFG "cfg"

bool LoadMappings(const char *dev_type, int port, const std::string& joyname, std::vector<uint16_t>& mappings, bool (&inverted)[3], int (&initial)[3])
{
	assert(JOY_MAPS_COUNT == countof(JoystickMapNames));
	std::stringstream str;

	if (joyname.empty())
		return false;

	int j = 0;
	mappings.resize(JOY_MAPS_COUNT);
	for (auto& i: mappings)
	{
		str.clear();
		str.str("");
		str << "map_" << JoystickMapNames[j++];
		const std::string& name = str.str();
		int32_t var;
		if (LoadSetting(dev_type, port, joyname, name.c_str(), var))
			i = var;
		else
			i = -1;
	}

	for (int i=0; i<3; i++)
	{
		str.clear(); str.str("");
		str << "inverted_" << JoystickMapNames[JOY_STEERING + i];
		{
			const std::string& name = str.str();
			if (!LoadSetting(dev_type, port, joyname, name.c_str(), inverted[i]))
				inverted[i] = false;
		}

		str.clear(); str.str("");
		str << "initial_" << JoystickMapNames[JOY_STEERING + i];
		{
			const std::string& name = str.str();
			if (!LoadSetting(dev_type, port, joyname, name.c_str(), initial[i]))
				initial[i] = 0;
		}
	}
	return true;
}

bool SaveMappings(const char *dev_type, int port, const std::string& joyname, const std::vector<uint16_t>& mappings, const bool (&inverted)[3], int (&initial)[3])
{
	assert(JOY_MAPS_COUNT == countof(JoystickMapNames));
	if (joyname.empty() || mappings.size() != JOY_MAPS_COUNT)
		return false;

	std::stringstream str;
	for (int i=0; i<JOY_MAPS_COUNT; i++)
	{
		str.clear();
		str.str("");
		str << "map_" << JoystickMapNames[i];
		const std::string& name = str.str();
		if (!SaveSetting(dev_type, port, joyname, name.c_str(), static_cast<int32_t>(mappings[i])))
			return false;
	}

	for (int i=0; i<3; i++)
	{
		str.clear(); str.str("");
		str << "inverted_" << JoystickMapNames[JOY_STEERING + i];
		{
			const std::string& name = str.str();
			if (!SaveSetting(dev_type, port, joyname, name.c_str(), inverted[i]))
				return false;
		}

		str.clear(); str.str("");
		str << "initial_" << JoystickMapNames[JOY_STEERING + i];
		{
			const std::string& name = str.str();
			if (!SaveSetting(dev_type, port, joyname, name.c_str(), initial[i]))
				return false;
		}
	}
	return true;
}

bool LoadBuzzMappings(const char *dev_type, int port, const std::string& joyname, std::vector<uint16_t>& mappings)
{
	std::stringstream str;

	if (joyname.empty())
		return false;

	int j = 0;

	mappings.resize(countof(buzz_map_names) * 4);
	for (auto& i: mappings)
	{
		str.str("");
		str.clear();
		str << "map_" << buzz_map_names[j % 5] << "_" << (j / 5);
		const std::string& name = str.str();
		int32_t var;
		if (LoadSetting(dev_type, port, joyname, name.c_str(), var))
			i = var;
		else
			i = -1;
		j++;
	}
	return true;
}

bool SaveBuzzMappings(const char *dev_type, int port, const std::string& joyname, const std::vector<uint16_t>& mappings)
{
	if (joyname.empty())
		return false;

	std::stringstream str;

	const size_t c = countof(buzz_map_names);
	for (size_t i=0; i < mappings.size(); i++)
	{
		str.str("");
		str.clear();
		str << "map_" << buzz_map_names[i % c] << "_" << (i / c);
		const std::string& name = str.str();
		if (!SaveSetting(dev_type, port, joyname, name.c_str(), static_cast<int32_t>(mappings[i])))
			return false;
	}
	return true;
}

static void refresh_store(ConfigData *cfg)
{
	GtkTreeIter iter;

	gtk_list_store_clear (cfg->store);
	for (auto& it: cfg->jsconf)
	{
		for (int i = 0; /*i < JOY_MAPS_COUNT && */i < it.second.mappings.size(); i++)
		{
			if (it.second.mappings[i] == (uint16_t)-1)
				continue;

			const char *pc_name = "Unknown";
			cfg->cb->get_event_name(cfg->dev_type, i, it.second.mappings[i], &pc_name);

			gtk_list_store_append (cfg->store, &iter);

			if (!strcmp(cfg->dev_type, BuzzDevice::TypeName())) {

				std::stringstream ss;
				ss << (1 + i / countof(buzz_map_names));
				ss << " ";
				ss << buzz_map_names[i % countof(buzz_map_names)];

				std::string name = ss.str();

				gtk_list_store_set (cfg->store, &iter,
					COL_NAME, it.first.c_str(),
					COL_PS2, name.c_str(),
					COL_PC, pc_name,
					COL_COLUMN_WIDTH, 50,
					COL_BINDING, i,
					-1);
			} else {
				gtk_list_store_set (cfg->store, &iter,
					COL_NAME, it.first.c_str(),
					COL_PS2, JoystickMapNames[i],
					COL_PC, pc_name,
					COL_COLUMN_WIDTH, 50,
					COL_BINDING, i,
					-1);
			}
		}
	}
}

static void joystick_changed (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int port = reinterpret_cast<uintptr_t> (data);
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);

	if (!cfg)
		return;

	if (idx > -1) {
		std::string name = (cfg->joysticks.begin() + idx)->first;
		cfg->js_iter = (cfg->joysticks.begin() + idx);
		OSDebugOut("Selected player %d idx: %d dev: '%s'\n", 2 - port, idx, name.c_str());
	}
}

static void button_clicked (GtkComboBox *widget, gpointer data)
{
	int port = reinterpret_cast<uintptr_t> (data);
	int type = reinterpret_cast<uintptr_t> (g_object_get_data (G_OBJECT (widget), JOYTYPE));
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);

	if (cfg /*&& type < cfg->mappings.size() && cfg->js_iter != cfg->joysticks.end()*/)
	{
		int value, initial = 0;
		std::string dev_name;
		bool inverted = false;
		bool is_axis = (type >= JOY_STEERING && type <= JOY_BRAKE);

		gtk_label_set_text (GTK_LABEL (cfg->label), "Polling for input for 5 seconds...");
		OSDebugOut("Polling: isaxis:%d %d\n" , is_axis, type);

		// let label change its text
		while (gtk_events_pending ())
			gtk_main_iteration_do (FALSE);

		if (cfg->cb->poll(cfg->jsconf, dev_name, is_axis, value, inverted, initial))
		{
			auto it = std::find_if(cfg->jsconf.begin(), cfg->jsconf.end(),
				[&dev_name](auto& i)->bool {
					return i.first == dev_name;
				});

			if (it != cfg->jsconf.end() && type < it->second.mappings.size())
			{
				it->second.mappings[type] = value;
				if (is_axis) {
					it->second.inverted[type - JOY_STEERING] = inverted;
					it->second.initial[type - JOY_STEERING] = initial;
				}
				refresh_store(cfg);
			}
		}
		gtk_label_set_text (GTK_LABEL (cfg->label), "");
	}
}

static void button_clicked_buzz (GtkComboBox *widget, gpointer data)
{
	int port = reinterpret_cast<uintptr_t> (data);
	int type = reinterpret_cast<uintptr_t> (g_object_get_data (G_OBJECT (widget), JOYTYPE));
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);

	if (cfg /*&& type < cfg->mappings.size() && cfg->js_iter != cfg->joysticks.end()*/)
	{
		int value, initial = 0;
		std::string dev_name;
		bool inverted = false;

		gtk_label_set_text (GTK_LABEL (cfg->label), "Polling for input for 5 seconds...");
		OSDebugOut("Polling: %s\n", buzz_map_names[type]);

		// let label change its text
		while (gtk_events_pending ())
			gtk_main_iteration_do (FALSE);

		if (cfg->cb->poll(cfg->jsconf, dev_name, false, value, inverted, initial))
		{
			auto it = std::find_if(cfg->jsconf.begin(), cfg->jsconf.end(),
				[&dev_name](auto& i)->bool {
					return i.first == dev_name;
				});

			if (it != cfg->jsconf.end() && type < it->second.mappings.size())
			{
				OSDebugOut("setting mappings for %s %s_%d=%d\n", dev_name.c_str(),
					buzz_map_names[type % countof(buzz_map_names)],
					type / countof(buzz_map_names), value);
				it->second.mappings[type] = value;
				refresh_store(cfg);
			}
		}
		gtk_label_set_text (GTK_LABEL (cfg->label), "");
	}
}

// save references to row paths, automatically updated when store changes
static void view_selected_foreach_func (GtkTreeModel  *model,
	GtkTreePath *path, GtkTreeIter *iter, gpointer userdata)
{
	GList **rr_list = (GList **)userdata;
	GtkTreeRowReference *rowref;
	rowref = gtk_tree_row_reference_new(model, path);
	*rr_list = g_list_append(*rr_list, rowref);
}

static void view_remove_binding (GtkTreeModel *model,
	GtkTreeIter *iter, ConfigData *cfg)
{
	gchar *dev_name;
	int binding;

	gtk_tree_model_get (model, iter, COL_NAME, &dev_name, COL_BINDING, &binding, -1);
	OSDebugOut("name: %s, bind: %d\n", dev_name, binding);

	auto& js = cfg->jsconf;
	auto it = std::find_if(js.begin(), js.end(),
		[&dev_name](auto i){
			return i.first == dev_name;
		});
	if (it != js.end()) {
		it->second.mappings[binding] = (uint16_t)-1;
		OSDebugOut("Delete binding '%d' for '%s'\n", binding, it->first.c_str());
	}
	gtk_list_store_remove (GTK_LIST_STORE(model), iter);
	//refresh_store(cfg);

	g_free(dev_name);
}

static void clear_binding_clicked (GtkWidget *widget, gpointer data)
{
	GtkTreeModel *model = nullptr;
	GList *rr_list = nullptr;
	GList *node = nullptr;

	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(cfg->treeview);

	gtk_tree_selection_selected_foreach(sel, view_selected_foreach_func, &rr_list);

	/* // single row selection
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
		view_selected_foreach_func(model, nullptr, &iter, cfg);
	}*/

	GList *list = gtk_tree_selection_get_selected_rows (sel, &model);
	// remove rows from store pointed to by row references
	for (node = g_list_first(rr_list); node != nullptr; node = node->next) {
		GtkTreePath *path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
		if (path)
		{
			GtkTreeIter iter;

			if (gtk_tree_model_get_iter(model, &iter, path))
			{
				view_remove_binding (model, &iter, cfg);
				//gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
			}
		}
	};

	g_list_free_full(rr_list, (GDestroyNotify) gtk_tree_row_reference_free);
	g_list_free_full(list, (GDestroyNotify) gtk_tree_path_free);
}

static void clear_all_clicked (GtkWidget *widget, gpointer data)
{
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);
	for (auto& it: cfg->jsconf)
		it.second.mappings.assign(it.second.mappings.size(), -1);
	refresh_store(cfg);
}

static void hidraw_toggled (GtkToggleButton *widget, gpointer data)
{
	int port = reinterpret_cast<uintptr_t> (data);
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);
	if (cfg) {
		cfg->use_hidraw_ff_pt = (bool) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	}
}

int GtkPadConfigure(int port, const char* dev_type, const char *apititle, const char *apiname, GtkWindow *parent, ApiCallbacks& apicbs)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb;
	GtkWidget *main_hbox, *right_vbox, *left_vbox, *treeview;
	GtkWidget *button;

	int fd;
	ConfigData cfg;

	apicbs.populate(cfg.joysticks);

	cfg.js_iter = cfg.joysticks.end();
	cfg.label = gtk_label_new ("");
	cfg.store = gtk_list_store_new (NUM_COLS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
	cfg.cb = &apicbs;
	cfg.dev_type = dev_type;

	for (const auto& it: cfg.joysticks) {
		if ((fd = open(it.second.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
		{
			OSDebugOut("Cannot open device: %s\n", it.second.c_str());
			continue;
		}

		ConfigMapping c; c.fd = fd;
		LoadMappings (cfg.dev_type, port, it.first, c.mappings, c.inverted, c.initial);
		cfg.jsconf.push_back(std::make_pair(it.first, c));
		OSDebugOut("mappings for '%s': %d\n", it.first.c_str(), c.mappings.size());
	}

	refresh_store(&cfg);

	std::string path;
	LoadSetting(dev_type, port, apiname, N_JOYSTICK, path);

	cfg.use_hidraw_ff_pt = false;
	bool is_evdev = (strncmp(apiname, "evdev", 5) == 0);
	if (is_evdev) //TODO idk about joydev
	{
		LoadSetting(dev_type, port, apiname, N_HIDRAW_FF_PT, cfg.use_hidraw_ff_pt);
	}

	// ---------------------------
	std::string title = (port ? "Player One " : "Player Two ");
	title += apititle;

	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		title.c_str(), parent, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);
	gtk_window_set_default_size (GTK_WINDOW(dlg), 320, 240);

	// ---------------------------
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (dlg));

	main_hbox = gtk_hbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER(dlg_area_box), main_hbox);

	left_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (main_hbox), left_vbox, TRUE, TRUE, 5);
	right_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (main_hbox), right_vbox, TRUE, TRUE, 5);

	// ---------------------------
	treeview = gtk_tree_view_new ();
	cfg.treeview = GTK_TREE_VIEW(treeview);
	auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	GtkCellRenderer *render = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1, "Name", render, "text", COL_NAME, "width", COL_COLUMN_WIDTH, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1, "PS2", render, "text", COL_PS2, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1, "PC", render, "text", COL_PC, NULL);

	gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW(treeview), 0);

	gtk_tree_view_column_set_resizable (gtk_tree_view_get_column(GTK_TREE_VIEW (treeview), 0), TRUE);
	gtk_tree_view_column_set_resizable (gtk_tree_view_get_column(GTK_TREE_VIEW (treeview), 1), TRUE);
	gtk_tree_view_column_set_resizable (gtk_tree_view_get_column(GTK_TREE_VIEW (treeview), 2), TRUE);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (cfg.store));
	g_object_unref (GTK_TREE_MODEL (cfg.store)); //treeview has its own ref

	GtkWidget *scwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scwin), treeview);
	//gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scwin), 200);
	gtk_widget_set_size_request (GTK_WIDGET(scwin), 200, 100);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin), GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_box_pack_start (GTK_BOX (left_vbox), scwin, TRUE, TRUE, 5);

	button = gtk_button_new_with_label ("Clear binding");
	gtk_box_pack_start (GTK_BOX (left_vbox), button, FALSE, FALSE, 5);
	g_object_set_data (G_OBJECT (button), CFG, &cfg);
	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (clear_binding_clicked), reinterpret_cast<gpointer> (port));

	button = gtk_button_new_with_label ("Clear All");
	gtk_box_pack_start (GTK_BOX (left_vbox), button, FALSE, FALSE, 5);
	g_object_set_data (G_OBJECT (button), CFG, &cfg);
	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (clear_all_clicked), reinterpret_cast<gpointer> (port));


	// ---------------------------

	// Remapping
	{
		GtkWidget* table = gtk_table_new (5, 7, true);
		gtk_container_add (GTK_CONTAINER(right_vbox), table);
		//GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default
		GtkAttachOptions opt = (GtkAttachOptions)(GTK_FILL);

		const char* button_labels[] = {
			"L2", "L1 / L", "R2", "R1 / R / Orange",
			"Left", "Up", "Right", "Down",
			"Square / X / Green", "Cross / A / Blue", "Circle / B / Red", "Triangle / Y / Yellow",
			"Select", "Start",
		};

		const Point button_pos[] = {
			{1, 0, JOY_L2},
			{1, 1, JOY_L1},
			{5, 0, JOY_R2},
			{5, 1, JOY_R1},
			{0, 3, JOY_LEFT},
			{1, 2, JOY_UP},
			{2, 3, JOY_RIGHT},
			{1, 4, JOY_DOWN},
			{4, 3, JOY_SQUARE},
			{5, 4, JOY_CROSS},
			{6, 3, JOY_CIRCLE},
			{5, 2, JOY_TRIANGLE},
			{3, 3, JOY_SELECT},
			{3, 2, JOY_START},
		};

		for (int i=0; i<countof(button_labels); i++)
		{
			GtkWidget *button = gtk_button_new_with_label (button_labels[i]);
			g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked), reinterpret_cast<gpointer> (port));

			g_object_set_data (G_OBJECT (button), JOYTYPE, reinterpret_cast<gpointer> (button_pos[i].type));
			g_object_set_data (G_OBJECT (button), CFG, &cfg);

			gtk_table_attach (GTK_TABLE (table), button,
					0 + button_pos[i].x, 1 + button_pos[i].x,
					0 + button_pos[i].y, 1 + button_pos[i].y,
					opt, opt, 5, 1);
		}

		GtkWidget *hbox = gtk_hbox_new (false, 5);
		gtk_container_add (GTK_CONTAINER (right_vbox), hbox);

		button = gtk_button_new_with_label ("Steering");
		gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), JOYTYPE, reinterpret_cast<gpointer> (JOY_STEERING));
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked), reinterpret_cast<gpointer> (port));

		button = gtk_button_new_with_label ("Throttle");
		gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), JOYTYPE, reinterpret_cast<gpointer> (JOY_THROTTLE));
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked), reinterpret_cast<gpointer> (port));

		button = gtk_button_new_with_label ("Brake");
		gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), JOYTYPE, reinterpret_cast<gpointer> (JOY_BRAKE));
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked), reinterpret_cast<gpointer> (port));

		gtk_box_pack_start (GTK_BOX (right_vbox), cfg.label, TRUE, TRUE, 5);
	}

	if (is_evdev)
	{
		ro_frame = gtk_frame_new ("Logitech wheel force feedback pass-through using hidraw");
		gtk_box_pack_start (GTK_BOX (right_vbox), ro_frame, FALSE, FALSE, 5);

		GtkWidget *frame_vbox = gtk_vbox_new (FALSE, 5);
		gtk_container_add (GTK_CONTAINER (ro_frame), frame_vbox);

		GtkWidget *chk_btn = gtk_check_button_new_with_label("Enable");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_btn), (gboolean)cfg.use_hidraw_ff_pt);
		g_object_set_data (G_OBJECT (chk_btn), CFG, &cfg);
		g_signal_connect (G_OBJECT (chk_btn), "toggled", G_CALLBACK (hidraw_toggled), reinterpret_cast<gpointer> (port));
		gtk_box_pack_start (GTK_BOX(frame_vbox), chk_btn, FALSE, FALSE, 5);

		rs_cb = new_combobox ("Device:", frame_vbox);

		int idx = 0, sel_idx = 0;
		for (auto& it : cfg.joysticks)
		{
			std::stringstream str;
			str << it.first;
			if (strcmp(apiname, "evdev") && !it.second.empty())
				str << " [" << it.second << "]";

			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), str.str().c_str ());
			if (path == it.second)
				sel_idx = idx;
			idx++;
		}

		g_object_set_data (G_OBJECT (rs_cb), CFG, &cfg);
		g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (joystick_changed), reinterpret_cast<gpointer> (port));
		gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);
	}
	// ---------------------------
	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));

	int ret = RESULT_OK;
	if (result == GTK_RESPONSE_OK)
	{
		if (cfg.js_iter != cfg.joysticks.end()) {
			if (!SaveSetting(dev_type, port, apiname, N_JOYSTICK, cfg.js_iter->second))
				ret = RESULT_FAILED;
		}

		for (auto& it: cfg.jsconf)
			SaveMappings(dev_type, port, it.first, it.second.mappings, it.second.inverted, it.second.initial);

		if (is_evdev) {
			SaveSetting(dev_type, port, apiname, N_HIDRAW_FF_PT, cfg.use_hidraw_ff_pt);
		}
	}
	else
		ret = RESULT_CANCELED;

	for (auto& it: cfg.jsconf)
		close(it.second.fd);

	gtk_widget_destroy (dlg);
	return ret;
}

GtkWidget * make_color_icon(uint32_t rgb)
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 24, 24);
	guchar *data = gdk_pixbuf_get_pixels (pixbuf);

	for (size_t i=0; i<24*24; i++) {
		data[i * 4 + 0] = rgb & 0xFF;
		data[i * 4 + 1] = (rgb >> 8) & 0xFF;
		data[i * 4 + 2] = (rgb >> 16) & 0xFF;
		data[i * 4 + 3] = icon_buzz_24[i];
	}

	GtkWidget *w = gtk_image_new_from_pixbuf (pixbuf);
	g_object_unref (G_OBJECT(pixbuf));
	return w;
}

int GtkBuzzConfigure(int port, const char* dev_type, const char *apititle, const char *apiname, GtkWindow *parent, ApiCallbacks& apicbs)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb;
	GtkWidget *main_hbox, *right_vbox, *left_vbox, *treeview;
	GtkWidget *button;

	int fd;
	ConfigData cfg;

	apicbs.populate(cfg.joysticks);

	cfg.js_iter = cfg.joysticks.end();
	cfg.label = gtk_label_new ("");
	cfg.store = gtk_list_store_new (NUM_COLS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
	cfg.cb = &apicbs;
	cfg.dev_type = dev_type;

	for (const auto& it: cfg.joysticks) {
		if ((fd = open(it.second.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
		{
			OSDebugOut("Cannot open device: %s\n", it.second.c_str());
			continue;
		}

		ConfigMapping c; c.fd = fd;
		LoadBuzzMappings (cfg.dev_type, port, it.first, c.mappings);
		cfg.jsconf.push_back(std::make_pair(it.first, c));
		OSDebugOut("mappings for '%s': %d\n", it.first.c_str(), c.mappings.size());
	}

	refresh_store(&cfg);

	// ---------------------------
	std::string title = "Buzz ";
	title += apititle;

	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		title.c_str(), parent, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);
	gtk_window_set_default_size (GTK_WINDOW(dlg), 320, 240);

	// ---------------------------
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (dlg));

	main_hbox = gtk_hbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER(dlg_area_box), main_hbox);

	left_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (main_hbox), left_vbox, TRUE, TRUE, 5);
	right_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (main_hbox), right_vbox, TRUE, TRUE, 5);

	// ---------------------------
	treeview = gtk_tree_view_new ();
	cfg.treeview = GTK_TREE_VIEW(treeview);
	auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	GtkCellRenderer *render = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1, "Name", render, "text", COL_NAME, "width", COL_COLUMN_WIDTH, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1, "PS2", render, "text", COL_PS2, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1, "PC", render, "text", COL_PC, NULL);

	gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW(treeview), 0);

	gtk_tree_view_column_set_resizable (gtk_tree_view_get_column(GTK_TREE_VIEW (treeview), 0), TRUE);
	gtk_tree_view_column_set_resizable (gtk_tree_view_get_column(GTK_TREE_VIEW (treeview), 1), TRUE);
	gtk_tree_view_column_set_resizable (gtk_tree_view_get_column(GTK_TREE_VIEW (treeview), 2), TRUE);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (cfg.store));
	g_object_unref (GTK_TREE_MODEL (cfg.store)); //treeview has its own ref

	GtkWidget *scwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scwin), treeview);
	gtk_widget_set_size_request (GTK_WIDGET(scwin), 200, 100);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin), GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_box_pack_start (GTK_BOX (left_vbox), scwin, TRUE, TRUE, 5);

	button = gtk_button_new_with_label ("Clear binding");
	gtk_box_pack_start (GTK_BOX (left_vbox), button, FALSE, FALSE, 5);
	g_object_set_data (G_OBJECT (button), CFG, &cfg);
	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (clear_binding_clicked), reinterpret_cast<gpointer> (port));

	button = gtk_button_new_with_label ("Clear All");
	gtk_box_pack_start (GTK_BOX (left_vbox), button, FALSE, FALSE, 5);
	g_object_set_data (G_OBJECT (button), CFG, &cfg);
	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (clear_all_clicked), reinterpret_cast<gpointer> (port));


	// ---------------------------

	// Remapping
	{
		GtkWidget* table = gtk_table_new (5, 4, true);
		gtk_container_add (GTK_CONTAINER(right_vbox), table);
		GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default

		const char* button_labels[] {
			"Red",
			"Blue",
			"Orange",
			"Green",
			"Yellow",
		};

		Buzz buzz_btns[] {
			BUZZ_RED,
			BUZZ_BLUE,
			BUZZ_ORANGE,
			BUZZ_GREEN,
			BUZZ_YELLOW,
		};

		uint32_t icon_colors[] {
			0x0000FF,
			0xFF0000,
			0x0080FF,
			0x00FF00,
			0x00FFFF,
		};

		for (int j=0; j < 4; j++) {
			for (int i=0; i < countof(button_labels); i++)
			{
				GtkWidget *button = gtk_button_new_with_label (button_labels[i]);


				GtkWidget *icon = make_color_icon(icon_colors[i]);
				gtk_button_set_image (GTK_BUTTON(button), icon);
				gtk_button_set_image_position (GTK_BUTTON(button), GTK_POS_LEFT);

				GList *children = gtk_container_get_children(GTK_CONTAINER(button));
				//OSDebugOut("widget: %s\n", gtk_widget_get_name(GTK_WIDGET(children->data)));

				//Gtk 3.16+
				//gtk_label_set_xalign (GTK_WIDGET(children), 0.0)

				//gtk_misc_set_alignment (GTK_MISC (children->data), 0.0, 0.5);
				if (GTK_IS_ALIGNMENT (children->data))
					gtk_alignment_set (GTK_ALIGNMENT (children->data), 0.0f, 0.5f, 0.2f, 0.f);

				g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked_buzz), reinterpret_cast<gpointer> (port));

				g_object_set_data (G_OBJECT (button), JOYTYPE, reinterpret_cast<gpointer> (j * countof(buzz_btns) + buzz_btns[i]));
				g_object_set_data (G_OBJECT (button), CFG, &cfg);

				gtk_table_attach (GTK_TABLE (table), button,
						j, 1 + j,
						i + 1, 2 + i,
						opt, opt, 5, 1);
			}

			gtk_table_attach (GTK_TABLE (table), gtk_label_new ("Player 1"),
								0, 1, 0, 1, opt, opt, 5, 1);
			gtk_table_attach (GTK_TABLE (table), gtk_label_new ("Player 2"),
								1, 2, 0, 1, opt, opt, 5, 1);
			gtk_table_attach (GTK_TABLE (table), gtk_label_new ("Player 3"),
								2, 3, 0, 1, opt, opt, 5, 1);
			gtk_table_attach (GTK_TABLE (table), gtk_label_new ("Player 4"),
								3, 4, 0, 1, opt, opt, 5, 1);

		}

		GtkWidget *hbox = gtk_hbox_new (false, 5);
		gtk_container_add (GTK_CONTAINER (right_vbox), hbox);

		gtk_box_pack_start (GTK_BOX (right_vbox), cfg.label, TRUE, TRUE, 5);
	}

	// ---------------------------
	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));

	int ret = RESULT_OK;
	if (result == GTK_RESPONSE_OK)
	{
		if (cfg.js_iter != cfg.joysticks.end()) {
			if (!SaveSetting(dev_type, port, apiname, N_JOYSTICK, cfg.js_iter->second))
				ret = RESULT_FAILED;
		}

		for (auto& it: cfg.jsconf)
			SaveBuzzMappings(dev_type, port, it.first, it.second.mappings);

	}
	else
		ret = RESULT_CANCELED;

	for (auto& it: cfg.jsconf)
		close(it.second.fd);

	gtk_widget_destroy (dlg);
	return ret;
}

}} //namespace
