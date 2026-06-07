#include "MapperViewerWindow.h"
#include "Bus.h"
#include "Cartridge.h"
#include "Mapper.h"

MapperViewerWindow::MapperViewerWindow(Bus* bus, QWidget* parent)
    : QDialog(parent), bus(bus)
{
    setWindowTitle("Mapper Viewer");
    resize(680, 760);

    text = new QPlainTextEdit(this);
    text->setReadOnly(true);
    text->setFocusPolicy(Qt::NoFocus);
    text->setFont(QFont("Consolas", 10));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(text);
    setLayout(layout);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MapperViewerWindow::updateInfo);
    timer->start(200);

    updateInfo();
}

void MapperViewerWindow::updateInfo()
{
    if (!bus || !bus->cart || !bus->cart->pMapper)
    {
        text->setPlainText("Chưa nạp ROM hoặc chưa có mapper.");
        return;
    }

    QString s;

    s += "===== TRÌNH XEM MAPPER =====\n\n";
    s += QString("Mapper ID : %1\n").arg(bus->cart->nMapperID);
    s += QString("Submapper : %1\n").arg(bus->cart->pMapper->nSubmapper);
    s += "\n";

    s += bus->cart->pMapper->GetDebugInfo();

    text->setPlainText(s);
}