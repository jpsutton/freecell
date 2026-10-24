#include "dialogs.h"

#include <QDialog>
#include <QMessageBox>
#include <QPointer>

#ifdef FREECELL_TOUCH_UI
#include <QEvent>
#include <QKeyEvent>
#endif

namespace {

#ifdef FREECELL_TOUCH_UI
// A dimmed layer over the whole window that holds one dialog as a child
// widget. It also swallows every touch outside the dialog, which makes the
// dialog modal.
class Veil : public QWidget {
public:
    Veil(QWidget* top, QDialog* dlg) : QWidget(top), m_top(top), m_dlg(dlg) {
        setObjectName(QStringLiteral("veil"));
        setAttribute(Qt::WA_StyledBackground);
        setStyleSheet(QStringLiteral("#veil { background: rgba(0, 0, 0, 150); }"));
        top->installEventFilter(this);
        dlg->installEventFilter(this);
        dlg->setParent(this, Qt::Widget);
        dlg->setObjectName(QStringLiteral("embeddedDialog"));
        dlg->setAttribute(Qt::WA_StyledBackground);
        dlg->setAutoFillBackground(true);
        dlg->setStyleSheet(QStringLiteral("#embeddedDialog { background: palette(window); border: 1px solid palette(mid); }"));
        dlg->adjustSize();
        place();
        show();
        raise();
        dlg->show();
        place();  // some dialogs (QMessageBox) size themselves when shown
        dlg->setFocus();
    }

    void place() {
        setGeometry(m_top->rect());
        QSize s = m_dlg->size();
        // Wide enough for thumbs: nine tenths of the window, never past its edges.
        s.setWidth(qMin(qMax(s.width(), width() * 9 / 10), width() - 32));
        s.setHeight(qMin(s.height(), height() - 32));
        m_dlg->resize(s);
        m_dlg->move((width() - s.width()) / 2, (height() - s.height()) / 2);
    }

protected:
    bool eventFilter(QObject* o, QEvent* e) override {
        if (o == m_top && e->type() == QEvent::Resize) place();
        // Android's back button.
        if (o == m_dlg && e->type() == QEvent::KeyPress) {
            int k = static_cast<QKeyEvent*>(e)->key();
            if (k == Qt::Key_Back || k == Qt::Key_Escape) {
                m_dlg->reject();
                return true;
            }
        }
        return false;
    }

private:
    QWidget* m_top;
    QDialog* m_dlg;
};
#endif

}  // namespace

namespace dialogs {

void open(QWidget* host, QDialog* dlg, std::function<void(int)> done) {
#ifdef FREECELL_TOUCH_UI
    QWidget* top = host->window();
    auto* veil = new Veil(top, dlg);
    QObject::connect(dlg, &QDialog::finished, veil, [veil, done](int result) {
        veil->hide();
        if (done) done(result);
        veil->deleteLater();
    });
#else
    Q_UNUSED(host)
    if (done) QObject::connect(dlg, &QDialog::finished, dlg, [done](int result) { done(result); });
    QObject::connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
    dlg->open();
#endif
}

void ask(QWidget* host, const QString& title, const QString& text, std::function<void(bool)> done) {
    auto* box = new QMessageBox(QMessageBox::Question, title, text,
                                QMessageBox::Yes | QMessageBox::No, host);
    box->setEscapeButton(QMessageBox::No);
    open(host, box, [done](int r) {
        if (done) done(r == QMessageBox::Yes);
    });
}

void tell(QWidget* host, const QString& title, const QString& text) {
    auto* box = new QMessageBox(QMessageBox::Information, title, text, QMessageBox::Ok, host);
    open(host, box);
}

}  // namespace dialogs
