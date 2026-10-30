#include "impactwidgets.h"

#include "analysers/analyserwidgets.h"
#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QJsonObject>
#include <QLocale>
#include <QVector2D>

#include <algorithm>
#include <cmath>

namespace tibia::analysers {

namespace {

using A = input::EGameAction;

bool isPremiumPlayer()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player && player->isPremium();
}

QString percentString(const double fraction) { return QString::number(fraction * 100.0, 'f', 1); }

/// CIP 0x100bfbd70: a per second value, rounded and cut to 32 bits.
QString rateString(const double value) { return format::multi(static_cast<quint32>(static_cast<qint64>(std::nearbyint(value)))); }

void copyToClipboard(const QString& text)
{
    auto action = input::mutableClone(*input::makeAction(A::COPY_TRUSTED_TEXT_TO_CLIPBOARD));
    action->text = text;
    input::publish(action);
}

QJsonObject readOptions(config::TSidebarOptions* options, const char* key)
{
    return options ? options->section(QLatin1String(key)).toObject() : QJsonObject{};
}

template <class Widget>
void applyContentHeight(Widget* widget, config::TSidebarOptions* options, const char* key)
{
    const QJsonObject json = readOptions(options, key);
    if (json.contains(QLatin1String("contentHeight")))
        widget->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                    json.value(QLatin1String("contentMaximized")).toBool(true));
}

template <class Widget>
void storeContentHeight(Widget* widget, config::TSidebarOptions* options, const char* key)
{
    if (!options)
        return;
    QJsonObject json = readOptions(options, key);
    json.insert(QStringLiteral("contentHeight"), widget->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget->isMaximized());
    options->setSection(QLatin1String(key), json);
}

/// The graphs' x in minutes: one point per 4 s, or per minute with session values.
QVariantList graphPoints(const double* values, const int count, const bool sessionValues)
{
    const double step = sessionValues ? 1.0 : 4.0 / 60.0;
    QVariantList points;
    points.reserve(count);
    for (int i = 0; i < count; ++i)
        points.append(QVector2D(static_cast<float>(-(count - 1 - i) * step), static_cast<float>(values[i])));
    return points;
}

constexpr int TypeName = Qt::UserRole + 1;
constexpr int TypeImage = Qt::UserRole + 2;
constexpr int Value = Qt::UserRole + 3;
constexpr int ValueString = Qt::UserRole + 4;
constexpr int PercentageFromTotalString = Qt::UserRole + 5;
constexpr int TypeIdentifier = Qt::UserRole + 6;

}

// ----------------------------------------------------------------------------------------------
// TDamageTypedModel / TDamageSourceModel

TDamageTypedModel::TDamageTypedModel(const bool damageInputRoles, QObject* parent)
    : QAbstractListModel(parent)
    , m_damageInputRoles(damageInputRoles)
{}

int TDamageTypedModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_entries.size()); }

QVariant TDamageTypedModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const Entry& entry = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case TypeName: return format::damageTypeName(entry.element);
        case TypeImage: return format::damageTypeIcon(entry.element);
        case Value: return entry.value;
        case ValueString: return format::grouped(entry.value);
        case PercentageFromTotalString: return percentString(entry.fraction);
        case TypeIdentifier: return entry.element;
        default: return {};
    }
}

QHash<int, QByteArray> TDamageTypedModel::roleNames() const
{
    return {
        { TypeName, "typeName" },
        { TypeImage, "typeImage" },
        { Value, m_damageInputRoles ? "damageValue" : "value" },
        { ValueString, m_damageInputRoles ? "damageValueString" : "valueString" },
        { PercentageFromTotalString, "percentageFromTotalString" },
        { TypeIdentifier, "typeIdentifier" },
    };
}

void TDamageTypedModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

int TDamageSourceModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_entries.size()); }

QVariant TDamageSourceModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const Entry& entry = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case Qt::UserRole + 1:
        case Qt::UserRole + 5: return entry.name;
        case Qt::UserRole + 2: return entry.value;
        case Qt::UserRole + 3: return format::grouped(entry.value);
        case Qt::UserRole + 4: return percentString(entry.fraction);
        default: return {};
    }
}

QHash<int, QByteArray> TDamageSourceModel::roleNames() const
{
    return {
        { Qt::UserRole + 1, "sourceName" },
        { Qt::UserRole + 2, "damageValue" },
        { Qt::UserRole + 3, "damageValueString" },
        { Qt::UserRole + 4, "percentageFromTotalString" },
        { Qt::UserRole + 5, "typeIdentifier" },
    };
}

void TDamageSourceModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

// ----------------------------------------------------------------------------------------------
// TImpactWidgetController

namespace {

constexpr auto ImpactOptionsKey = "impactAnalyserWidgetOptions";

}

TImpactWidgetController::TImpactWidgetController(TImpactStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_typedModel(new TDamageTypedModel(false, this))
{
    m_dpsGraphMaxY = 1.0;
    m_hpsGraphMaxY = 1.0;
    connect(m_storage, &TImpactStorage::damageValuesChanged, this, [this] {
        updateTypedModel();
        Q_EMIT damageValuesChanged();
    });
    connect(m_storage, &TImpactStorage::healingValuesChanged, this, &TImpactWidgetController::healingValuesChanged);
    connect(m_storage, &TImpactStorage::allTimeValuesChanged, this, &TImpactWidgetController::allTimeValuesChanged);
    connect(m_storage, &TImpactStorage::graphValuesChanged, this, &TImpactWidgetController::updateGraphs);
    connect(m_storage, &TImpactStorage::sessionLengthChanged, this, &TImpactWidgetController::sessionLengthChanged);
    connect(m_storage, &TImpactStorage::optionsChanged, this, [this] {
        updateTypedModel();
        updateGraphs();
        Q_EMIT optionsChanged();
        Q_EMIT damageValuesChanged();
        Q_EMIT healingValuesChanged();
        Q_EMIT sessionLengthChanged();
    });
    m_hooks.emplace_back("object", "onPremiumChange", [this](LuaObject* self, const std::vector<std::any>&) {
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && self == player.get())
            Q_EMIT optionsChanged();
        return std::any{};
    });
    updateTypedModel();
    updateGraphs();
    widget()->createItem(QStringLiteral("ImpactWidget.qml"));
}

TImpactWidgetController::~TImpactWidgetController() { delete m_targetDialog; }

void TImpactWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    applyContentHeight(widget(), m_options, ImpactOptionsKey);
}

void TImpactWidgetController::saveWidgetOptions() { storeContentHeight(widget(), m_options, ImpactOptionsKey); }

void TImpactWidgetController::updateTypedModel()
{
    const TImpactStorage::Summary& summary =
        m_storage->showSessionValues() ? m_storage->session(TImpactStorage::Damage) : m_storage->window(TImpactStorage::Damage);
    std::vector<TDamageTypedModel::Entry> entries;
    for (int e = 0; e < TImpactStorage::ElementCount; ++e) {
        const qint64 value = summary.byElement[static_cast<size_t>(e)];
        if (value != 0 && summary.total != 0)
            entries.push_back({ e, value, static_cast<double>(value) / static_cast<double>(summary.total) });
    }
    m_typedModel->setEntries(std::move(entries));
}

void TImpactWidgetController::updateGraphs()
{
    const bool session = m_storage->showSessionValues();
    const auto& damage = m_storage->graph(TImpactStorage::Damage);
    const auto& healing = m_storage->graph(TImpactStorage::Healing);
    m_dpsGraphData = graphPoints(damage.data(), static_cast<int>(damage.size()), session);
    m_hpsGraphData = graphPoints(healing.data(), static_cast<int>(healing.size()), session);
    m_dpsGraphMaxY = adjustGraphMaxY(m_dpsGraphMaxY, *std::max_element(damage.begin(), damage.end()));
    m_hpsGraphMaxY = adjustGraphMaxY(m_hpsGraphMaxY, *std::max_element(healing.begin(), healing.end()));
    Q_EMIT graphValuesChanged();
}

double TImpactWidgetController::perSecond(const TImpactStorage::Kind kind) const
{
    return m_storage->showSessionValues() ? m_storage->sessionPerSecond(kind) : m_storage->perSecond(kind);
}

QString TImpactWidgetController::damageSumString() const
{
    const TImpactStorage::Kind kind = TImpactStorage::Damage;
    return format::multi(m_storage->showSessionValues() ? m_storage->session(kind).total : m_storage->window(kind).total);
}

QString TImpactWidgetController::healingSumString() const
{
    const TImpactStorage::Kind kind = TImpactStorage::Healing;
    return format::multi(m_storage->showSessionValues() ? m_storage->session(kind).total : m_storage->window(kind).total);
}

QString TImpactWidgetController::maxDpsString() const { return rateString(m_storage->maxPerSecond(TImpactStorage::Damage)); }

QString TImpactWidgetController::maxHpsString() const { return rateString(m_storage->maxPerSecond(TImpactStorage::Healing)); }

double TImpactWidgetController::damagePerSecond() const { return perSecond(TImpactStorage::Damage); }

double TImpactWidgetController::healingPerSecond() const { return perSecond(TImpactStorage::Healing); }

QString TImpactWidgetController::dpsString() const { return rateString(damagePerSecond()); }

QString TImpactWidgetController::hpsString() const { return rateString(healingPerSecond()); }

bool TImpactWidgetController::isDpsGaugeVisible() const { return m_storage->gaugeVisible(TImpactStorage::Damage) && isPremiumPlayer(); }

bool TImpactWidgetController::isHpsGaugeVisible() const { return m_storage->gaugeVisible(TImpactStorage::Healing) && isPremiumPlayer(); }

double TImpactWidgetController::dpsTargetValue() const { return m_storage->gaugeTarget(TImpactStorage::Damage); }

double TImpactWidgetController::hpsTargetValue() const { return m_storage->gaugeTarget(TImpactStorage::Healing); }

QString TImpactWidgetController::dpsTargetValueString() const { return rateString(dpsTargetValue()); }

QString TImpactWidgetController::hpsTargetValueString() const { return rateString(hpsTargetValue()); }

bool TImpactWidgetController::isDpsGraphVisible() const { return m_storage->graphVisible(TImpactStorage::Damage) && isPremiumPlayer(); }

bool TImpactWidgetController::isHpsGraphVisible() const { return m_storage->graphVisible(TImpactStorage::Healing) && isPremiumPlayer(); }

QString TImpactWidgetController::damageAllTimeMaxString() const { return format::multi(m_storage->allTimeMax(TImpactStorage::Damage)); }

QString TImpactWidgetController::healingAllTimeMaxString() const { return format::multi(m_storage->allTimeMax(TImpactStorage::Healing)); }

bool TImpactWidgetController::isDamageTypesVisible() const { return m_storage->damageTypesVisible(); }

bool TImpactWidgetController::showSessionValues() const { return m_storage->showSessionValues(); }

/// CIP 0x100ccb450: resets, the period switch, both gauges, the type list, both graphs.
void TImpactWidgetController::onShowContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("impact_reset")), &gamewindow::TContextMenuEntryItem::triggered, this, [this] { m_storage->reset(); });
    connect(menu->addCustomEntry(qtTrId("impact_reset_all_time")), &gamewindow::TContextMenuEntryItem::triggered, this, [this] {
        m_storage->resetAllTimeMax();
    });
    const bool session = m_storage->showSessionValues();
    connect(menu->addCustomEntry(qtTrId(session ? "impact_show_recent_values" : "impact_show_session_values")),
            &gamewindow::TContextMenuEntryItem::triggered, this, [this, session] { m_storage->setShowSessionValues(!session); });
    menu->addCustomSeparator();
    addGaugeEntries(TImpactStorage::Damage);
    addGaugeEntries(TImpactStorage::Healing);
    menu->addCustomSeparator();
    gamewindow::TContextMenuEntryItem* types = menu->addCustomEntry(qtTrId("impact_show_damage_types"));
    types->setCheckable(true);
    types->setChecked(m_storage->damageTypesVisible());
    connect(types, &gamewindow::TContextMenuEntryItem::toggled, this, [this](const bool checked) { m_storage->setDamageTypesVisible(checked); });
    menu->addCustomSeparator();
    addGraphEntry(TImpactStorage::Damage);
    addGraphEntry(TImpactStorage::Healing);
    menu->showCustomMenu();
}

/// CIP 0x100ccbb30 / 0x100ccbe90.
void TImpactWidgetController::addGaugeEntries(const TImpactStorage::Kind kind)
{
    auto* menu = gamewindow::TContextMenuController::instance();
    const bool premium = isPremiumPlayer();
    const bool damage = kind == TImpactStorage::Damage;
    if (premium) {
        connect(menu->addCustomEntry(qtTrId(damage ? "impact_set_dps_target" : "impact_set_hps_target")), &gamewindow::TContextMenuEntryItem::triggered,
                this, [this, kind] { showTargetDialog(kind); });
    }
    gamewindow::TContextMenuEntryItem* gauge =
        menu->addCustomEntry(qtTrId(damage ? "impact_show_dps_gauge" : "impact_show_hps_gauge"), premium ? QString() : qtTrId("context_premium_feature"));
    gauge->setCheckable(true);
    gauge->setChecked(m_storage->gaugeVisible(kind) && premium);
    connect(gauge, &gamewindow::TContextMenuEntryItem::toggled, this, [this, kind](const bool checked) { m_storage->setGaugeVisible(kind, checked); });
}

/// CIP 0x100ccc0d0 / 0x100ccc430.
void TImpactWidgetController::addGraphEntry(const TImpactStorage::Kind kind)
{
    auto* menu = gamewindow::TContextMenuController::instance();
    const bool premium = isPremiumPlayer();
    gamewindow::TContextMenuEntryItem* graph = menu->addCustomEntry(
        qtTrId(kind == TImpactStorage::Damage ? "impact_show_dps_graph" : "impact_show_hps_graph"), premium ? QString() : qtTrId("context_premium_feature"));
    graph->setCheckable(true);
    graph->setChecked(m_storage->graphVisible(kind) && premium);
    connect(graph, &gamewindow::TContextMenuEntryItem::toggled, this, [this, kind](const bool checked) { m_storage->setGraphVisible(kind, checked); });
}

void TImpactWidgetController::onShowDpsGaugeContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    addGaugeEntries(TImpactStorage::Damage);
    menu->showCustomMenu();
}

void TImpactWidgetController::onShowHpsGaugeContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    addGaugeEntries(TImpactStorage::Healing);
    menu->showCustomMenu();
}

void TImpactWidgetController::onShowDpsGraphContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    addGraphEntry(TImpactStorage::Damage);
    menu->showCustomMenu();
}

void TImpactWidgetController::onShowHpsGraphContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    addGraphEntry(TImpactStorage::Healing);
    menu->showCustomMenu();
}

void TImpactWidgetController::setDesiredIsDpsGaugeVisible(const bool Visible) { m_storage->setGaugeVisible(TImpactStorage::Damage, Visible); }

void TImpactWidgetController::setDesiredIsHpsGaugeVisible(const bool Visible) { m_storage->setGaugeVisible(TImpactStorage::Healing, Visible); }

void TImpactWidgetController::setDesiredIsDpsGraphVisible(const bool Visible) { m_storage->setGraphVisible(TImpactStorage::Damage, Visible); }

void TImpactWidgetController::setDesiredIsHpsGraphVisible(const bool Visible) { m_storage->setGraphVisible(TImpactStorage::Healing, Visible); }

/// CIP 0x100cc9ae0 / 0x100cc9b50: a whole number, negative read as 0.
void TImpactWidgetController::onDpsTargetValueSelected(const QString& DpsTargetString)
{
    bool ok = false;
    const int value = DpsTargetString.toInt(&ok);
    if (ok)
        m_storage->setGaugeTarget(TImpactStorage::Damage, std::max(0, value));
}

void TImpactWidgetController::onHpsTargetValueSelected(const QString& HpsTargetString)
{
    bool ok = false;
    const int value = HpsTargetString.toInt(&ok);
    if (ok)
        m_storage->setGaugeTarget(TImpactStorage::Healing, std::max(0, value));
}

void TImpactWidgetController::showDpsTargetSelectionDialog() { showTargetDialog(TImpactStorage::Damage); }

void TImpactWidgetController::showHpsTargetSelectionDialog() { showTargetDialog(TImpactStorage::Healing); }

void TImpactWidgetController::showTargetDialog(const TImpactStorage::Kind kind)
{
    delete m_targetDialog;
    const bool damage = kind == TImpactStorage::Damage;
    m_targetDialog = new gamewindow::TGenericEnterTextDialogController();
    m_targetDialog->setCaption(qtTrId(damage ? "impact_dps_target_input_caption" : "impact_hps_target_input_caption"));
    m_targetDialog->setDescription(qtTrId(damage ? "impact_dps_target_input_description" : "impact_hps_target_input_description"));
    m_targetDialog->setMaxInputLength(9);
    m_targetDialog->setText(QString::number(static_cast<qint64>(m_storage->gaugeTarget(kind))));
    connect(m_targetDialog, &gamewindow::TGenericEnterTextDialogController::textEntered, this,
            damage ? &TImpactWidgetController::onDpsTargetValueSelected : &TImpactWidgetController::onHpsTargetValueSelected);
    m_targetDialog->dialog()->show();
}

// ----------------------------------------------------------------------------------------------
// TDamageInputWidgetController

namespace {

constexpr auto DamageInputOptionsKey = "damageInputAnalyserWidgetOptions";

QString sourceLabel(const QString& name) { return name.isEmpty() ? qtTrId("damageinput_other_damage_source") : name; }

}

TDamageInputWidgetController::TDamageInputWidgetController(TDamageInputStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                           QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_sources(new TDamageSourceModel(this))
    , m_types(new TDamageTypedModel(true, this))
    , m_fromSource(new TDamageTypedModel(true, this))
{
    connect(m_storage, &TDamageInputStorage::valuesChanged, this, &TDamageInputWidgetController::updateLists);
    connect(m_storage, &TDamageInputStorage::graphChanged, this, &TDamageInputWidgetController::updateGraph);
    connect(m_storage, &TDamageInputStorage::optionsChanged, this, [this] {
        updateLists();
        updateGraph();
        Q_EMIT widgetConfigurationChanged();
    });
    updateLists();
    updateGraph();
    widget()->createItem(QStringLiteral("DamageInputWidget.qml"));
}

void TDamageInputWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    applyContentHeight(widget(), m_options, DamageInputOptionsKey);
}

void TDamageInputWidgetController::saveWidgetOptions() { storeContentHeight(widget(), m_options, DamageInputOptionsKey); }

bool TDamageInputWidgetController::showDamageGraph() const { return m_storage->showGraph(); }

bool TDamageInputWidgetController::showDamageTypeList() const { return m_storage->showTypes(); }

bool TDamageInputWidgetController::showDamageSourceList() const { return m_storage->showSources(); }

bool TDamageInputWidgetController::showSessionValues() const { return m_storage->showSessionValues(); }

/// CIP 0x100cd36b0 / 0x100cd3900.
QString TDamageInputWidgetController::totalDamageString() const { return format::grouped(m_storage->total()); }

QString TDamageInputWidgetController::maxDpsTakenString() const { return format::grouped(m_storage->maxPerSecond()); }

void TDamageInputWidgetController::updateLists()
{
    const double total = static_cast<double>(m_storage->total());
    std::vector<TDamageTypedModel::Entry> types;
    const auto& elements = m_storage->elements();
    for (int e = 0; e < TDamageInputStorage::ElementCount; ++e) {
        const qint64 value = elements[static_cast<size_t>(e)];
        if (value != 0)
            types.push_back({ e, value, total > 0 ? static_cast<double>(value) / total : 0.0 });
    }
    m_types->setEntries(std::move(types));
    std::vector<TDamageSourceModel::Entry> sources;
    for (const TDamageInputStorage::Source& source : m_storage->sources())
        sources.push_back({ sourceLabel(source.name), source.total, total > 0 ? static_cast<double>(source.total) / total : 0.0 });
    m_sources->setEntries(std::move(sources));
    updateDetail();
    Q_EMIT damageTakenValuesChanged();
}

void TDamageInputWidgetController::updateDetail()
{
    if (!m_detailedSource.isEmpty()) {
        for (const TDamageInputStorage::Source& source : m_storage->sources()) {
            if (sourceLabel(source.name) != m_detailedSource)
                continue;
            std::vector<TDamageTypedModel::Entry> entries;
            for (int e = 0; e < TDamageInputStorage::ElementCount; ++e) {
                const qint64 value = source.byElement[static_cast<size_t>(e)];
                if (value != 0 && source.total != 0)
                    entries.push_back({ e, value, static_cast<double>(value) / static_cast<double>(source.total) });
            }
            m_fromSource->setEntries(std::move(entries));
            return;
        }
        m_detailedSource.clear();
        Q_EMIT detailedDamageSourceChanged();
    }
    m_fromSource->setEntries({});
}

void TDamageInputWidgetController::updateGraph()
{
    const std::vector<double> values = m_storage->graph(m_graphMaxY);
    m_graph = graphPoints(values.data(), static_cast<int>(values.size()), m_storage->showSessionValues());
    Q_EMIT damageTakenGraphChanged();
}

void TDamageInputWidgetController::onDamageSourceDetailsRequested(const QString& DamageSourceName)
{
    m_detailedSource = DamageSourceName == m_detailedSource ? QString() : DamageSourceName;
    Q_EMIT detailedDamageSourceChanged();
    updateDetail();
}

/// CIP 0x100cd3ad0.
void TDamageInputWidgetController::onShowContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("damageinput_reset")), &gamewindow::TContextMenuEntryItem::triggered, this, [this] { m_storage->reset(); });
    const bool session = m_storage->showSessionValues();
    connect(menu->addCustomEntry(qtTrId(session ? "damageinput_show_recent_values" : "damageinput_show_session_values")),
            &gamewindow::TContextMenuEntryItem::triggered, this, [this, session] { m_storage->setShowSessionValues(!session); });
    menu->addCustomSeparator();
    const auto checkable = [this, menu](const char* text, const bool checked, void (TDamageInputStorage::*set)(bool)) {
        gamewindow::TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(text));
        entry->setCheckable(true);
        entry->setChecked(checked);
        connect(entry, &gamewindow::TContextMenuEntryItem::toggled, this, [this, set](const bool value) { (m_storage->*set)(value); });
    };
    checkable("damageinput_show_graph", m_storage->showGraph(), &TDamageInputStorage::setShowGraph);
    checkable("damageinput_show_types_list", m_storage->showTypes(), &TDamageInputStorage::setShowTypes);
    checkable("damageinput_show_sources_list", m_storage->showSources(), &TDamageInputStorage::setShowSources);
    menu->addCustomSeparator();
    connect(menu->addCustomEntry(qtTrId("copy_to_clipboard")), &gamewindow::TContextMenuEntryItem::triggered, this,
            [this] { copyToClipboard(exportText()); });
    menu->showCustomMenu();
}

/// CIP 0x100cd4c60: the summary, the type list, the source list and the selected source's types.
QString TDamageInputWidgetController::exportText() const
{
    QStringList lines;
    lines.append(qtTrId("damageinput_summary_caption"));
    lines.append(QStringLiteral("%1 %2").arg(qtTrId("damageinput_total"), totalDamageString()));
    lines.append(QStringLiteral("%1 %2").arg(qtTrId("damageinput_maximum"), maxDpsTakenString()));
    const QString row = QStringLiteral("  %1 %2 (%3%)");
    lines.append(qtTrId("damageinput_damagetype_caption"));
    for (const TDamageTypedModel::Entry& entry : m_types->entries())
        lines.append(row.arg(format::damageTypeName(entry.element), format::grouped(entry.value), percentString(entry.fraction)));
    if (m_types->entries().empty())
        lines.append(qtTrId("damageinput_nodata"));
    lines.append(qtTrId("damageinput_damagesource_caption"));
    for (const TDamageSourceModel::Entry& entry : m_sources->entries())
        lines.append(row.arg(entry.name, format::grouped(entry.value), percentString(entry.fraction)));
    if (m_sources->entries().empty())
        lines.append(qtTrId("damageinput_nodata"));
    if (!m_fromSource->entries().empty()) {
        lines.append(m_detailedSource);
        for (const TDamageTypedModel::Entry& entry : m_fromSource->entries())
            lines.append(row.arg(format::damageTypeName(entry.element), format::grouped(entry.value), percentString(entry.fraction)));
    }
    return lines.join(QLatin1Char('\n'));
}

// ----------------------------------------------------------------------------------------------
// TPartyHuntMembersListModel / TPartyHuntWidgetController

int TPartyHuntMembersListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_members.size()); }

QVariant TPartyHuntMembersListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_members.size()))
        return {};
    const TPartyHuntStorage::Member& member = m_members[static_cast<size_t>(index.row())];
    switch (role) {
        case Name: return member.name;
        case CharacterId: return member.id;
        case IsLeader: return member.id != 0 && member.id == m_leaderId;
        case IsInParty: return member.active;
        case Gain: return member.loot;
        case Waste: return member.supplies;
        case Balance: return member.loot - member.supplies;
        case Damage: return member.damage;
        case Healing: return member.healing;
        default: return {};
    }
}

QHash<int, QByteArray> TPartyHuntMembersListModel::roleNames() const
{
    return {
        { Name, "name" },       { CharacterId, "characterID" }, { IsLeader, "isLeader" }, { IsInParty, "isInParty" }, { Gain, "gain" },
        { Waste, "waste" },     { Balance, "balance" },         { Damage, "damage" },     { Healing, "healing" },
    };
}

void TPartyHuntMembersListModel::setMembers(const std::vector<TPartyHuntStorage::Member>& members, const quint32 leaderId)
{
    beginResetModel();
    m_members = members;
    m_leaderId = leaderId;
    endResetModel();
}

namespace {

constexpr auto PartyHuntOptionsKey = "partyHuntAnalyserOptions";

}

TPartyHuntWidgetController::TPartyHuntWidgetController(TPartyHuntStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_model(new TPartyHuntMembersListModel(this))
    , m_proxy(new gamewindow::TPartyHuntMembersListModelSortFilterProxy(this))
{
    m_proxy->setSourceModel(m_model);
    m_proxy->sort(0);
    m_partyMemberModel = QVariant::fromValue<QObject*>(m_proxy);
    connect(m_storage, &TPartyHuntStorage::changed, this, &TPartyHuntWidgetController::onPartyHuntDataChanged);
    onPartyHuntDataChanged();
    widget()->createItem(QStringLiteral("PartyHuntWidget.qml"));
}

void TPartyHuntWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    applyContentHeight(widget(), m_options, PartyHuntOptionsKey);
}

void TPartyHuntWidgetController::saveWidgetOptions() { storeContentHeight(widget(), m_options, PartyHuntOptionsKey); }

void TPartyHuntWidgetController::onPartyHuntDataChanged()
{
    m_model->setMembers(m_storage->members(), m_storage->leaderId());
    Q_EMIT partyHuntDataChanged();
    Q_EMIT sessionLengthChanged();
    Q_EMIT lootTypeChanged();
}

void TPartyHuntWidgetController::onPartyLeaderIDChanged() { onPartyHuntDataChanged(); }

QString TPartyHuntWidgetController::sessionLengthString() const
{
    const qint64 minutes = m_storage->sessionSeconds() / 60;
    return QStringLiteral("%1:%2h").arg(minutes / 60, 2, 10, QLatin1Char('0')).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

/// CIP 0x100d98370: price type 1 is the leader's prices.
QString TPartyHuntWidgetController::lootTypeString() const
{
    return qtTrId(m_storage->priceType() == 1 ? "partyhunt_loot_type_leader" : "partyhunt_loot_type_market");
}

qulonglong TPartyHuntWidgetController::totalGain() const
{
    qint64 sum = 0;
    for (const TPartyHuntStorage::Member& member : m_storage->members())
        sum += member.loot;
    return static_cast<qulonglong>(std::max<qint64>(0, sum));
}

qulonglong TPartyHuntWidgetController::totalWaste() const
{
    qint64 sum = 0;
    for (const TPartyHuntStorage::Member& member : m_storage->members())
        sum += member.supplies;
    return static_cast<qulonglong>(std::max<qint64>(0, sum));
}

qlonglong TPartyHuntWidgetController::totalBalance() const
{
    return static_cast<qlonglong>(totalGain()) - static_cast<qlonglong>(totalWaste());
}

/// CIP 0x100d98480: the leader resets the group's session and picks the price type; others
/// clear their local copy.
void TPartyHuntWidgetController::requestConfigurationContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    const bool leader = player && m_storage->leaderId() != 0 && player->getId() == m_storage->leaderId();
    if (leader) {
        connect(menu->addCustomEntry(qtTrId("partyhunt_rest_for_group")), &gamewindow::TContextMenuEntryItem::triggered, this,
                [] { g_game.sendPartyAnalyzerReset(); });
        connect(menu->addCustomEntry(qtTrId(m_storage->priceType() == 1 ? "partyhunt_set_loot_type_market" : "partyhunt_set_loot_type_leader")),
                &gamewindow::TContextMenuEntryItem::triggered, this, [] { g_game.sendPartyAnalyzerPriceType(); });
    } else {
        connect(menu->addCustomEntry(qtTrId("partyhunt_rest_local")), &gamewindow::TContextMenuEntryItem::triggered, this,
                [this] { m_storage->clear(); });
    }
    menu->addCustomSeparator();
    connect(menu->addCustomEntry(qtTrId("copy_to_clipboard")), &gamewindow::TContextMenuEntryItem::triggered, this,
            [this] { copyToClipboard(exportText()); });
    menu->showCustomMenu();
}

/// CIP 0x100d98e40.
QString TPartyHuntWidgetController::exportText() const
{
    QStringList lines;
    const auto line = [&lines](const QString& label, const QString& value) { lines.append(QStringLiteral("%1: %2").arg(label, value)); };
    line(qtTrId("huntinganalyser_sessionlength"), sessionLengthString());
    line(qtTrId("partyhunt_loot_type_label"), lootTypeString());
    line(qtTrId("huntinganalyser_gain"), format::grouped(static_cast<qint64>(totalGain())));
    line(qtTrId("huntinganalyser_waste"), format::grouped(static_cast<qint64>(totalWaste())));
    line(qtTrId("huntinganalyser_gain_waste_sum"), format::grouped(totalBalance()));
    for (const TPartyHuntStorage::Member& member : m_storage->members()) {
        lines.append(member.name);
        line(QStringLiteral("  ") + qtTrId("huntinganalyser_gain"), format::grouped(member.loot));
        line(QStringLiteral("  ") + qtTrId("huntinganalyser_waste"), format::grouped(member.supplies));
        line(QStringLiteral("  ") + qtTrId("huntinganalyser_gain_waste_sum"), format::grouped(member.loot - member.supplies));
        line(QStringLiteral("  ") + qtTrId("damage"), format::grouped(member.damage));
        line(QStringLiteral("  ") + qtTrId("healing"), format::grouped(member.healing));
    }
    return lines.join(QLatin1Char('\n'));
}

}
