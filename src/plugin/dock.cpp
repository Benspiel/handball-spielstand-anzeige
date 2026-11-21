#include "dock.hpp"

#include "camera_capture.hpp"
#include "preview_widget.hpp"
#include "scorebug.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace hbp {

namespace {

constexpr int kTickMs = 250;

QString u8(const char *s)
{
	return QString::fromUtf8(s);
}

QString colorStyle(uint32_t argb)
{
	const QColor c = QColor::fromRgba(argb);
	const bool bright = (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) > 170.0;
	return QString("background-color: %1; color: %2; padding: 4px 10px;")
		.arg(c.name())
		.arg(bright ? "#101218" : "#ffffff");
}

} // namespace

HandballDock::HandballDock(QWidget *parent) : QWidget(parent)
{
	const bool firstRun = !os_file_exists(configFilePath().c_str());
	cfg_ = loadConfig();
	buildUi();
	loadIntoUi();
	loading_ = false;

	startTime_ = os_gettime_ns() / 1e9;
	refreshCameras();
	applyConfig();

	tickTimer_ = new QTimer(this);
	tickTimer_->setInterval(kTickMs);
	connect(tickTimer_, &QTimer::timeout, this, &HandballDock::onTick);
	tickTimer_->start();

	saveTimer_ = new QTimer(this);
	saveTimer_->setSingleShot(true);
	saveTimer_->setInterval(600);
	connect(saveTimer_, &QTimer::timeout, this, &HandballDock::saveNow);

	// Datei sofort anlegen, damit "erster Start" nur genau einmal erkannt wird.
	if (firstRun)
		saveNow();
}

HandballDock::~HandballDock()
{
	shutdown();
}

void HandballDock::shutdown()
{
	if (shutDown_)
		return;
	shutDown_ = true;
	if (tickTimer_)
		tickTimer_->stop();
	saveNow();
	// Kamera freigeben, solange OBS' Grafik noch läuft.
	Config empty = cfg_;
	empty.cameraKind.clear();
	empty.cameraId.clear();
	engine_.setConfig(empty);
}

void HandballDock::buildUi()
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	outer->addWidget(scroll);

	auto *content = new QWidget();
	scroll->setWidget(content);
	auto *root = new QVBoxLayout(content);

	// --- Kamera ---------------------------------------------------------------------------------
	auto *camBox = new QGroupBox(u8("Kamera"), content);
	auto *camLayout = new QGridLayout(camBox);
	cameraBox_ = new QComboBox(camBox);
	cameraBox_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	cameraBox_->setMinimumContentsLength(12);
	refreshButton_ = new QPushButton(u8("Neu laden"), camBox);
	resolutionBox_ = new QComboBox(camBox);
	resolutionBox_->addItem(u8("Auflösung: Gerätestandard"), "");
	resolutionBox_->addItem("1280 x 720", "1280x720");
	resolutionBox_->addItem("1920 x 1080", "1920x1080");
	resolutionBox_->addItem("3840 x 2160", "3840x2160");
	detectBox_ = new QCheckBox(u8("Erkennung aktiv"), camBox);
	camLayout->addWidget(cameraBox_, 0, 0);
	camLayout->addWidget(refreshButton_, 0, 1);
	camLayout->addWidget(resolutionBox_, 1, 0, 1, 2);
	camLayout->addWidget(detectBox_, 2, 0, 1, 2);
	root->addWidget(camBox);

	// --- Vorschau -------------------------------------------------------------------------------
	preview_ = new PreviewWidget(content);
	preview_->setMinimumHeight(170);
	root->addWidget(preview_, 1);
	statusLabel_ = new QLabel(content);
	statusLabel_->setWordWrap(true);
	root->addWidget(statusLabel_);

	// --- Tabs -----------------------------------------------------------------------------------
	auto *tabs = new QTabWidget(content);

	// Tab 1: Spiel
	auto *gameTab = new QWidget();
	auto *gameLayout = new QVBoxLayout(gameTab);
	liveLabel_ = new QLabel(gameTab);
	QFont lf = liveLabel_->font();
	lf.setPointSize(lf.pointSize() + 3);
	lf.setBold(true);
	liveLabel_->setFont(lf);
	liveLabel_->setAlignment(Qt::AlignCenter);
	gameLayout->addWidget(liveLabel_);

	auto *teamForm = new QGridLayout();
	homeNameEdit_ = new QLineEdit(gameTab);
	awayNameEdit_ = new QLineEdit(gameTab);
	homeColorButton_ = new QPushButton(u8("Farbe"), gameTab);
	awayColorButton_ = new QPushButton(u8("Farbe"), gameTab);
	teamForm->addWidget(new QLabel(u8("Heim"), gameTab), 0, 0);
	teamForm->addWidget(homeNameEdit_, 0, 1);
	teamForm->addWidget(homeColorButton_, 0, 2);
	teamForm->addWidget(new QLabel(u8("Gast"), gameTab), 1, 0);
	teamForm->addWidget(awayNameEdit_, 1, 1);
	teamForm->addWidget(awayColorButton_, 1, 2);
	teamForm->setColumnStretch(1, 1);
	gameLayout->addLayout(teamForm);

	autoBox_ = new QCheckBox(u8("Tore und Halbzeit automatisch von der Tafel lesen"), gameTab);
	autoBox_->setToolTip(u8("Ausschalten, wenn du den Stand von Hand korrigieren willst."));
	gameLayout->addWidget(autoBox_);

	auto *manual = new QGridLayout();
	auto mk = [&](const QString &text, int hd, int ad) {
		auto *b = new QPushButton(text, gameTab);
		connect(b, &QPushButton::clicked, this, [this, hd, ad] { onManualScore(hd, ad); });
		return b;
	};
	manual->addWidget(new QLabel(u8("Heim"), gameTab), 0, 0);
	manual->addWidget(mk("−1", -1, 0), 0, 1);
	manual->addWidget(mk("+1", 1, 0), 0, 2);
	manual->addWidget(new QLabel(u8("Gast"), gameTab), 1, 0);
	manual->addWidget(mk("−1", 0, -1), 1, 1);
	manual->addWidget(mk("+1", 0, 1), 1, 2);
	manual->setColumnStretch(3, 1);
	gameLayout->addLayout(manual);

	halfBox_ = new QComboBox(gameTab);
	for (int h = 1; h <= 4; ++h)
		halfBox_->addItem(halfLabel(h), h);
	auto *halfRow = new QHBoxLayout();
	halfRow->addWidget(new QLabel(u8("Halbzeit"), gameTab));
	halfRow->addWidget(halfBox_, 1);
	gameLayout->addLayout(halfRow);

	auto *resetButton = new QPushButton(u8("Spiel zurücksetzen (0:0, 1. Halbzeit)"), gameTab);
	connect(resetButton, &QPushButton::clicked, this, &HandballDock::onResetMatch);
	gameLayout->addWidget(resetButton);
	gameLayout->addStretch(1);
	tabs->addTab(gameTab, u8("Spiel"));

	// Tab 2: Tafel einrichten
	auto *boardTab = new QWidget();
	auto *boardLayout = new QVBoxLayout(boardTab);
	auto *hint = new QLabel(u8("1. Feld wählen   2. Im Kamerabild einen Rahmen um genau dieses Feld der Anzeigetafel "
				   "ziehen. Der Rahmen darf etwas größer sein als die Ziffern.\n"
				   "Die erkannte Zahl erscheint am Rahmen. Steht dort ein ?, Rahmen neu ziehen oder die "
				   "Empfindlichkeit ändern."),
				boardTab);
	hint->setWordWrap(true);
	boardLayout->addWidget(hint);

	auto *boardForm = new QFormLayout();
	fieldBox_ = new QComboBox(boardTab);
	for (int i = 0; i < FieldCount; ++i)
		fieldBox_->addItem(u8(fieldLabel(i)), i);
	digitsSpin_ = new QSpinBox(boardTab);
	digitsSpin_->setRange(1, 4);
	clearFieldButton_ = new QPushButton(u8("Rahmen entfernen"), boardTab);
	sensitivitySlider_ = new QSlider(Qt::Horizontal, boardTab);
	sensitivitySlider_->setRange(15, 80);
	sensitivityLabel_ = new QLabel(boardTab);
	auto *sensRow = new QHBoxLayout();
	sensRow->addWidget(sensitivitySlider_, 1);
	sensRow->addWidget(sensitivityLabel_);
	slantSpin_ = new QSpinBox(boardTab);
	slantSpin_->setRange(-30, 30);
	slantSpin_->setSuffix(" %");
	slantSpin_->setToolTip(u8("Nur für kursive (schräge) Ziffern: etwa 10 bis 20."));
	halfMinutesSpin_ = new QSpinBox(boardTab);
	halfMinutesSpin_->setRange(5, 60);
	halfMinutesSpin_->setSuffix(u8(" Minuten"));

	boardForm->addRow(u8("Feld"), fieldBox_);
	boardForm->addRow(u8("Anzahl Ziffern"), digitsSpin_);
	boardForm->addRow(QString(), clearFieldButton_);
	boardForm->addRow(u8("Empfindlichkeit"), sensRow);
	boardForm->addRow(u8("Schräge Ziffern"), slantSpin_);
	boardForm->addRow(u8("Halbzeitdauer"), halfMinutesSpin_);
	boardLayout->addLayout(boardForm);
	boardLayout->addStretch(1);
	tabs->addTab(boardTab, u8("Tafel einrichten"));

	root->addWidget(tabs);

	// --- Verbindungen ---------------------------------------------------------------------------
	connect(refreshButton_, &QPushButton::clicked, this, &HandballDock::refreshCameras);
	connect(cameraBox_, &QComboBox::activated, this, &HandballDock::onCameraChosen);
	connect(resolutionBox_, &QComboBox::activated, this, &HandballDock::onCameraChosen);
	connect(detectBox_, &QCheckBox::toggled, this, &HandballDock::onSettingsEdited);
	connect(fieldBox_, &QComboBox::currentIndexChanged, this, &HandballDock::onFieldChosen);
	connect(digitsSpin_, &QSpinBox::valueChanged, this, &HandballDock::onSettingsEdited);
	connect(clearFieldButton_, &QPushButton::clicked, this, [this] {
		const int f = fieldBox_->currentData().toInt();
		cfg_.fields[f].rect = hb::NormRect();
		applyConfig();
		scheduleSave();
	});
	connect(sensitivitySlider_, &QSlider::valueChanged, this, &HandballDock::onSettingsEdited);
	connect(slantSpin_, &QSpinBox::valueChanged, this, &HandballDock::onSettingsEdited);
	connect(halfMinutesSpin_, &QSpinBox::valueChanged, this, &HandballDock::onSettingsEdited);
	connect(homeNameEdit_, &QLineEdit::textEdited, this, &HandballDock::onSettingsEdited);
	connect(awayNameEdit_, &QLineEdit::textEdited, this, &HandballDock::onSettingsEdited);
	connect(homeColorButton_, &QPushButton::clicked, this, [this] { onPickColor(true); });
	connect(awayColorButton_, &QPushButton::clicked, this, [this] { onPickColor(false); });
	connect(autoBox_, &QCheckBox::toggled, this, &HandballDock::onSettingsEdited);
	connect(halfBox_, &QComboBox::activated, this, &HandballDock::onManualHalf);
	connect(preview_, &PreviewWidget::rectDrawn, this, &HandballDock::onRectDrawn);
}

void HandballDock::loadIntoUi()
{
	detectBox_->setChecked(cfg_.detectionEnabled);
	autoBox_->setChecked(cfg_.scoreAutomatic);
	homeNameEdit_->setText(QString::fromStdString(cfg_.homeName));
	awayNameEdit_->setText(QString::fromStdString(cfg_.awayName));
	sensitivitySlider_->setValue(cfg_.sensitivity);
	sensitivityLabel_->setText(QString("%1 %").arg(cfg_.sensitivity));
	slantSpin_->setValue(cfg_.slantPercent);
	halfMinutesSpin_->setValue(cfg_.halfMinutes);
	const int ri = resolutionBox_->findData(QString::fromStdString(cfg_.resolution));
	resolutionBox_->setCurrentIndex(ri >= 0 ? ri : 0);
	fieldBox_->setCurrentIndex(0);
	digitsSpin_->setValue(cfg_.fields[0].digits);
	preview_->setActiveField(0);
	updateColorButtons();
}

void HandballDock::updateColorButtons()
{
	homeColorButton_->setStyleSheet(colorStyle(cfg_.homeColor));
	awayColorButton_->setStyleSheet(colorStyle(cfg_.awayColor));
}

void HandballDock::refreshCameras()
{
	const bool wasLoading = loading_;
	loading_ = true;
	cameraBox_->clear();
	cameraBox_->addItem(u8("— keine Kamera —"), "");

	int select = 0;
	const auto cameras = listCameras();
	for (const CameraEntry &c : cameras) {
		cameraBox_->addItem(QString::fromStdString(c.label), QString::fromStdString(c.kind + "|" + c.id));
		if (c.kind == cfg_.cameraKind && c.id == cfg_.cameraId)
			select = cameraBox_->count() - 1;
	}
	cameraBox_->setCurrentIndex(select);
	loading_ = wasLoading;
}

void HandballDock::onCameraChosen()
{
	if (loading_)
		return;
	const QString data = cameraBox_->currentData().toString();
	const int sep = data.indexOf('|');
	if (sep < 0) {
		cfg_.cameraKind.clear();
		cfg_.cameraId.clear();
	} else {
		cfg_.cameraKind = data.left(sep).toStdString();
		cfg_.cameraId = data.mid(sep + 1).toStdString();
	}
	cfg_.resolution = resolutionBox_->currentData().toString().toStdString();
	resolutionBox_->setEnabled(cfg_.cameraKind == "device");
	applyConfig();
	scheduleSave();
}

void HandballDock::onFieldChosen()
{
	if (loading_)
		return;
	const int f = fieldBox_->currentData().toInt();
	loading_ = true;
	digitsSpin_->setValue(cfg_.fields[f].digits);
	loading_ = false;
	preview_->setActiveField(f);
}

void HandballDock::onRectDrawn(int field, double x, double y, double w, double h)
{
	cfg_.fields[field].rect = hb::NormRect{x, y, w, h};
	applyConfig();
	scheduleSave();
}

void HandballDock::onSettingsEdited()
{
	if (loading_)
		return;
	cfg_.detectionEnabled = detectBox_->isChecked();
	cfg_.scoreAutomatic = autoBox_->isChecked();
	cfg_.homeName = homeNameEdit_->text().toStdString();
	cfg_.awayName = awayNameEdit_->text().toStdString();
	cfg_.sensitivity = sensitivitySlider_->value();
	sensitivityLabel_->setText(QString("%1 %").arg(cfg_.sensitivity));
	cfg_.slantPercent = slantSpin_->value();
	cfg_.halfMinutes = halfMinutesSpin_->value();
	cfg_.fields[fieldBox_->currentData().toInt()].digits = digitsSpin_->value();
	applyConfig();
	scheduleSave();
}

void HandballDock::onPickColor(bool home)
{
	uint32_t &target = home ? cfg_.homeColor : cfg_.awayColor;
	const QColor chosen = QColorDialog::getColor(QColor::fromRgba(target), this, u8("Teamfarbe wählen"));
	if (!chosen.isValid())
		return;
	target = chosen.rgba();
	updateColorButtons();
	applyConfig();
	scheduleSave();
}

void HandballDock::setManualMode()
{
	if (autoBox_->isChecked()) {
		const bool prev = loading_;
		loading_ = true;
		autoBox_->setChecked(false);
		loading_ = prev;
		cfg_.scoreAutomatic = false;
		applyConfig();
		scheduleSave();
	}
}

void HandballDock::onManualScore(int homeDelta, int awayDelta)
{
	setManualMode(); // Sonst würde die Tafel den Wert gleich wieder zurücksetzen.
	const hb::MatchState &s = engine_.state();
	engine_.setScore(std::max(0, s.home + homeDelta), std::max(0, s.away + awayDelta));
}

void HandballDock::onManualHalf(int index)
{
	setManualMode();
	engine_.setHalf(halfBox_->itemData(index).toInt());
}

void HandballDock::onResetMatch()
{
	const auto answer = QMessageBox::question(this, u8("Spiel zurücksetzen"),
						  u8("Spielstand auf 0:0 und 1. Halbzeit zurücksetzen?"));
	if (answer == QMessageBox::Yes)
		engine_.resetMatch();
}

void HandballDock::applyConfig()
{
	engine_.setConfig(cfg_);
	for (int i = 0; i < FieldCount; ++i)
		preview_->setFieldRect(i, cfg_.fields[i].rect, QString());
	resolutionBox_->setEnabled(cfg_.cameraKind == "device");
}

void HandballDock::scheduleSave()
{
	if (saveTimer_)
		saveTimer_->start();
}

void HandballDock::saveNow()
{
	saveConfig(cfg_);
}

void HandballDock::updateStatus()
{
	const hb::MatchState &s = engine_.state();

	QString text;
	if (cfg_.cameraKind.empty())
		text = u8("Wähle oben eine Kamera.");
	else if (!engine_.cameraError().empty())
		text = QString::fromStdString(engine_.cameraError());
	else if (!engine_.hasFrame())
		text = u8("Warte auf Kamerabild …");
	else {
		const Frame &f = engine_.lastFrame();
		text = u8("Bild %1×%2").arg(f.width).arg(f.height);
		bool anyRect = false;
		for (const FieldConfig &fc : cfg_.fields)
			anyRect = anyRect || fc.rect.valid();
		if (!anyRect)
			text += u8(" · Zeichne im Tab \"Tafel einrichten\" Rahmen um die Felder der Anzeigetafel.");
		else if (!cfg_.detectionEnabled)
			text += u8(" · Erkennung ist aus.");
		else
			text += s.signal ? u8(" · Tafel wird gelesen.") : u8(" · Tafel nicht lesbar – Rahmen und Empfindlichkeit prüfen.");
	}
	statusLabel_->setText(text);

	liveLabel_->setText(QString("%1  %2 : %3  %4   %5   %6")
				    .arg(QString::fromStdString(cfg_.homeName))
				    .arg(s.home)
				    .arg(s.away)
				    .arg(QString::fromStdString(cfg_.awayName))
				    .arg(formatClock(s.clockSeconds))
				    .arg(halfLabel(s.half)));

	const int hi = halfBox_->findData(s.half);
	if (hi >= 0 && halfBox_->currentIndex() != hi && !halfBox_->view()->isVisible())
		halfBox_->setCurrentIndex(hi);
}

void HandballDock::onTick()
{
	if (shutDown_)
		return;

	const double now = os_gettime_ns() / 1e9 - startTime_;
	engine_.step(now);

	if (engine_.hasFrame()) {
		const Frame &f = engine_.lastFrame();
		QImage full(f.bgra.data(), f.width, f.height, f.width * 4, QImage::Format_RGB32);
		preview_->setImage(full.width() > 960 ? full.scaledToWidth(960, Qt::FastTransformation) : full.copy());

		for (int i = 0; i < FieldCount; ++i) {
			const hb::FieldReading &r = engine_.lastReading(i);
			QString t;
			if (cfg_.fields[i].rect.valid()) {
				if (r.status == hb::ReadStatus::Ok) {
					t = (i == FieldMinutes || i == FieldSeconds) ? QString("%1").arg(r.value, 2, 10, QChar('0'))
										  : QString::number(r.value);
				} else if (r.status == hb::ReadStatus::Unreadable) {
					t = "?";
				} else {
					t = QString::fromUtf8("–");
				}
			}
			preview_->setFieldRect(i, cfg_.fields[i].rect, t);
		}
	} else {
		preview_->clearImage();
		if (cfg_.cameraKind.empty())
			preview_->setMessage(u8("Keine Kamera gewählt"));
		else if (!engine_.cameraError().empty())
			preview_->setMessage(QString::fromStdString(engine_.cameraError()));
		else
			preview_->setMessage(u8("Warte auf Kamerabild …"));
	}
	preview_->update();
	updateStatus();
}

} // namespace hbp
