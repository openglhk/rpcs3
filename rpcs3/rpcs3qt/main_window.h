#pragma once

#ifdef _WIN32
#include <QWinTHumbnailToolbar>
#include <QWinTHumbnailToolbutton>
#endif

#include <QActionGroup>
#include <QMainWindow>
#include <QIcon>
#include <QMimeData>

#include "update_manager.h"
#include "settings.h"
#include "Emu/System.h"

#include <memory>

class log_frame;
class debugger_frame;
class game_list_frame;
class gui_settings;
class emu_settings;
class persistent_settings;
class kernel_explorer;

struct gui_game_info;

enum class game_boot_result : u32;

namespace compat
{
	struct package_info;
}

namespace Ui
{
	class main_window;
}

class main_window : public QMainWindow
{
	Q_OBJECT

	std::unique_ptr<Ui::main_window> ui;

	bool m_sys_menu_opened = false;
	bool m_is_list_mode = true;
	bool m_save_slider_pos = false;
	int m_other_slider_pos = 0;

	QIcon m_app_icon;
	QIcon m_icon_play;
	QIcon m_icon_pause;
	QIcon m_icon_stop;
	QIcon m_icon_restart;
	QIcon m_icon_fullscreen_on;
	QIcon m_icon_fullscreen_off;

#ifdef _WIN32
	QIcon m_icon_thumb_play;
	QIcon m_icon_thumb_pause;
	QIcon m_icon_thumb_stop;
	QIcon m_icon_thumb_restart;
	QWinThumbnailToolBar *m_thumb_bar = nullptr;
	QWinThumbnailToolButton *m_thumb_playPause = nullptr;
	QWinThumbnailToolButton *m_thumb_stop = nullptr;
	QWinThumbnailToolButton *m_thumb_restart = nullptr;
#endif

	enum class drop_type
	{
		drop_error,
		drop_pkg,
		drop_pup,
		drop_rap_edat,
		drop_psf,
		drop_dir,
		drop_game,
		drop_rrc
	};

public:
	explicit main_window(std::shared_ptr<gui_settings> gui_settings, std::shared_ptr<emu_settings> emu_settings, std::shared_ptr<persistent_settings> persistent_settings, QWidget *parent = nullptr);
	~main_window();
	bool Init(bool with_cli_boot);
	QIcon GetAppIcon() const;
	bool OnMissingFw();
	void InstallPackages(QStringList file_paths = QStringList());
	void InstallPup(QString file_path = "");

Q_SIGNALS:
	void RequestLanguageChange(const QString& language);
	void RequestGlobalStylesheetChange();
	void RequestTrophyManagerRepaint();
	void NotifyEmuSettingsChange();

public Q_SLOTS:
	void OnEmuStop();
	void OnEmuRun(bool start_playtime) const;
	void OnEmuResume() const;
	void OnEmuPause() const;
	void OnEmuReady() const;

	void RepaintGui();
	void RetranslateUI(const QStringList& language_codes, const QString& language);

private Q_SLOTS:
	void OnPlayOrPause();
	void Boot(const std::string& path, const std::string& title_id = "", bool direct = false, bool add_only = false, cfg_mode config_mode = cfg_mode::custom, const std::string& config_path = "");
	void BootElf();
	void BootTest();
	void BootGame();
	void BootVSH();
	void BootRsxCapture(std::string path = "");
	void DecryptSPRXLibraries();
	static void show_boot_error(game_boot_result status);

	void SaveWindowState() const;
	void ConfigureGuiFromSettings();
	void SetIconSizeActions(int idx) const;
	void ResizeIcons(int index);

	void RemoveDiskCache();
	void RemoveFirmwareCache();
	void CreateFirmwareCache();

protected:
	void closeEvent(QCloseEvent *event) override;
	void keyPressEvent(QKeyEvent *keyEvent) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void dropEvent(QDropEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dragLeaveEvent(QDragLeaveEvent* event) override;

private:
	void RepaintToolBarIcons();
	void RepaintThumbnailIcons();
	void CreateActions();
	void CreateConnects();
	void CreateDockWindows();
	void EnableMenus(bool enabled) const;
	void ShowTitleBars(bool show) const;

	static bool InstallFileInExData(const std::string& extension, const QString& path, const std::string& filename);

	void HandlePackageInstallation(QStringList file_paths);

	void HandlePupInstallation(const QString& file_path, const QString& dir_path = "");
	void ExtractPup();

	void ExtractTar();
	void ExtractMSELF();

	static drop_type IsValidFile(const QMimeData& md, QStringList* drop_paths = nullptr);
	static void AddGamesFromDir(const QString& path);

	QAction* CreateRecentAction(const q_string_pair& entry, const uint& sc_idx);
	void BootRecentAction(const QAction* act);
	void AddRecentAction(const q_string_pair& entry);

	void UpdateLanguageActions(const QStringList& language_codes, const QString& language);

	static QString GetCurrentTitle();

	q_pair_list m_rg_entries;
	QList<QAction*> m_recent_game_acts;

	std::shared_ptr<gui_game_info> m_selected_game;

	QActionGroup* m_icon_size_act_group = nullptr;
	QActionGroup* m_list_mode_act_group = nullptr;
	QActionGroup* m_category_visible_act_group = nullptr;

	// Dockable widget frames
	QMainWindow *m_mw = nullptr;
	log_frame* m_log_frame = nullptr;
	debugger_frame* m_debugger_frame = nullptr;
	game_list_frame* m_game_list_frame = nullptr;
	kernel_explorer* m_kernel_explorer = nullptr;
	std::shared_ptr<gui_settings> m_gui_settings;
	std::shared_ptr<emu_settings> m_emu_settings;
	std::shared_ptr<persistent_settings> m_persistent_settings;

	update_manager m_updater;
	QAction* m_download_menu_action = nullptr;
};
