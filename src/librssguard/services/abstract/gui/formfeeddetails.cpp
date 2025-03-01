// For license of this file, see <project-root-folder>/LICENSE.md.

#include "services/abstract/gui/formfeeddetails.h"

#include "database/databasequeries.h"
#include "definitions/definitions.h"
#include "exceptions/applicationexception.h"
#include "gui/guiutilities.h"
#include "miscellaneous/iconfactory.h"
#include "miscellaneous/textfactory.h"
#include "services/abstract/rootitem.h"

#include <QMenu>
#include <QNetworkReply>
#include <QPair>
#include <QPushButton>
#include <QTextCodec>

FormFeedDetails::FormFeedDetails(ServiceRoot* service_root, QWidget* parent)
  : QDialog(parent), m_feed(nullptr), m_serviceRoot(service_root) {
  initialize();
  createConnections();
}

void FormFeedDetails::activateTab(int index) {
  m_ui->m_tabWidget->setCurrentIndex(index);
}

void FormFeedDetails::clearTabs() {
  m_ui->m_tabWidget->clear();
}

void FormFeedDetails::insertCustomTab(QWidget* custom_tab, const QString& title, int index) {
  m_ui->m_tabWidget->insertTab(index, custom_tab, title);
}

void FormFeedDetails::apply() {
  // Setup common data for the feed.
  m_feed->setAutoUpdateType(static_cast<Feed::AutoUpdateType>(m_ui->m_cmbAutoUpdateType
                                                                ->itemData(m_ui->m_cmbAutoUpdateType->currentIndex())
                                                                .toInt()));
  m_feed->setAutoUpdateInterval(int(m_ui->m_spinAutoUpdateInterval->value()));
  m_feed->setOpenArticlesDirectly(m_ui->m_cbOpenArticlesAutomatically->isChecked());
  m_feed->setIsRtl(m_ui->m_cbFeedRTL->isChecked());
  m_feed->setAddAnyDatetimeArticles(m_ui->m_cbAddAnyDateArticles->isChecked());
  m_feed->setDatetimeToAvoid(m_ui->m_gbAvoidOldArticles->isChecked() ? m_ui->m_dtDateTimeToAvoid->dateTime()
                                                                     : TextFactory::parseDateTime(0));
  m_feed->setIsSwitchedOff(m_ui->m_cbDisableFeed->isChecked());
  m_feed->setIsQuiet(m_ui->m_cbSuppressFeed->isChecked());

  if (!m_creatingNew) {
    // We need to make sure that common data are saved.
    QSqlDatabase database = qApp->database()->driver()->connection(metaObject()->className());

    DatabaseQueries::createOverwriteFeed(database, m_feed, m_serviceRoot->accountId(), m_feed->parent()->id());
  }
}

void FormFeedDetails::onAutoUpdateTypeChanged(int new_index) {
  Feed::AutoUpdateType auto_update_type =
    static_cast<Feed::AutoUpdateType>(m_ui->m_cmbAutoUpdateType->itemData(new_index).toInt());

  switch (auto_update_type) {
    case Feed::AutoUpdateType::DontAutoUpdate:
    case Feed::AutoUpdateType::DefaultAutoUpdate:
      m_ui->m_spinAutoUpdateInterval->setEnabled(false);
      break;

    default:
      m_ui->m_spinAutoUpdateInterval->setEnabled(true);
  }
}

void FormFeedDetails::createConnections() {
  connect(m_ui->m_buttonBox, &QDialogButtonBox::accepted, this, &FormFeedDetails::acceptIfPossible);
  connect(m_ui->m_cmbAutoUpdateType,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this,
          &FormFeedDetails::onAutoUpdateTypeChanged);

  connect(m_ui->m_cbAddAnyDateArticles, &QCheckBox::toggled, this, [this](bool checked) {
    m_ui->m_gbAvoidOldArticles->setEnabled(!checked);
  });
}

void FormFeedDetails::loadFeedData() {
  if (m_creatingNew) {
    GuiUtilities::applyDialogProperties(*this,
                                        qApp->icons()->fromTheme(QSL("application-rss+xml")),
                                        tr("Add new feed"));
  }
  else {
    GuiUtilities::applyDialogProperties(*this, m_feed->fullIcon(), tr("Edit \"%1\"").arg(m_feed->title()));
  }

  m_ui->m_cmbAutoUpdateType
    ->setCurrentIndex(m_ui->m_cmbAutoUpdateType->findData(QVariant::fromValue(int(m_feed->autoUpdateType()))));
  m_ui->m_spinAutoUpdateInterval->setValue(m_feed->autoUpdateInterval());
  m_ui->m_cbOpenArticlesAutomatically->setChecked(m_feed->openArticlesDirectly());
  m_ui->m_cbFeedRTL->setChecked(m_feed->isRtl());
  m_ui->m_cbAddAnyDateArticles->setChecked(m_feed->addAnyDatetimeArticles());
  m_ui->m_gbAvoidOldArticles->setChecked(m_feed->datetimeToAvoid().toMSecsSinceEpoch() > 0);
  m_ui->m_dtDateTimeToAvoid->setDateTime(m_feed->datetimeToAvoid());
  m_ui->m_cbDisableFeed->setChecked(m_feed->isSwitchedOff());
  m_ui->m_cbSuppressFeed->setChecked(m_feed->isQuiet());
}

void FormFeedDetails::acceptIfPossible() {
  try {
    apply();
    accept();
  }
  catch (const ApplicationException& ex) {
    qApp->showGuiMessage(Notification::Event::GeneralEvent,
                         {tr("Cannot save feed properties"),
                          tr("Cannot save changes: %1").arg(ex.message()),
                          QSystemTrayIcon::MessageIcon::Critical},
                         {},
                         {},
                         this);
  }
}

void FormFeedDetails::initialize() {
  m_ui.reset(new Ui::FormFeedDetails());
  m_ui->setupUi(this);

  m_ui->m_dtDateTimeToAvoid
    ->setDisplayFormat(qApp->localization()->loadedLocale().dateTimeFormat(QLocale::FormatType::ShortFormat));

  // Setup auto-update options.
  m_ui->m_spinAutoUpdateInterval->setMode(TimeSpinBox::Mode::MinutesSeconds);
  m_ui->m_spinAutoUpdateInterval->setValue(DEFAULT_AUTO_UPDATE_INTERVAL);
  m_ui->m_cmbAutoUpdateType->addItem(tr("Fetch articles using global interval"),
                                     QVariant::fromValue(int(Feed::AutoUpdateType::DefaultAutoUpdate)));
  m_ui->m_cmbAutoUpdateType->addItem(tr("Fetch articles every"),
                                     QVariant::fromValue(int(Feed::AutoUpdateType::SpecificAutoUpdate)));
  m_ui->m_cmbAutoUpdateType->addItem(tr("Disable auto-fetching of articles"),
                                     QVariant::fromValue(int(Feed::AutoUpdateType::DontAutoUpdate)));
}
