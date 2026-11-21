#pragma once
// Das Dock in OBS: Kamera wählen, Tafelbereiche markieren, Spiel steuern.

#include "engine.hpp"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

namespace hbp {

class PreviewWidget;

class HandballDock : public QWidget {
	Q_OBJECT
public:
	explicit HandballDock(QWidget *parent = nullptr);
	~HandballDock() override;

	// Beim Beenden von OBS: Timer stoppen, Kamera freigeben, Einstellungen sichern.
	void shutdown();

private slots:
	void onTick();
	void refreshCameras();
	void onCameraChosen();
	void onFieldChosen();
	void onRectDrawn(int field, double x, double y, double w, double h);
	void onSettingsEdited();
	void onPickColor(bool home);
	void onManualScore(int homeDelta, int awayDelta);
	void onManualHalf(int index);
	void onResetMatch();
	void saveNow();

private:
	void buildUi();
	void loadIntoUi();
	void applyConfig();
	void scheduleSave();
	void updateColorButtons();
	void updateStatus();
	void setManualMode();

	Engine engine_;
	Config cfg_;
	bool loading_ = true;
	bool shutDown_ = false;
	double startTime_ = 0.0;

	QTimer *tickTimer_ = nullptr;
	QTimer *saveTimer_ = nullptr;

	PreviewWidget *preview_ = nullptr;
	QComboBox *cameraBox_ = nullptr;
	QComboBox *resolutionBox_ = nullptr;
	QPushButton *refreshButton_ = nullptr;
	QCheckBox *detectBox_ = nullptr;
	QLabel *statusLabel_ = nullptr;

	QComboBox *fieldBox_ = nullptr;
	QSpinBox *digitsSpin_ = nullptr;
	QPushButton *clearFieldButton_ = nullptr;
	QSlider *sensitivitySlider_ = nullptr;
	QLabel *sensitivityLabel_ = nullptr;
	QSpinBox *slantSpin_ = nullptr;
	QSpinBox *halfMinutesSpin_ = nullptr;

	QLineEdit *homeNameEdit_ = nullptr;
	QLineEdit *awayNameEdit_ = nullptr;
	QPushButton *homeColorButton_ = nullptr;
	QPushButton *awayColorButton_ = nullptr;

	QLabel *liveLabel_ = nullptr;
	QCheckBox *autoBox_ = nullptr;
	QComboBox *halfBox_ = nullptr;
};

} // namespace hbp
