
#include "renderarea.h"
#include <QPainter>
#include <QDebug>
#include "boardfacory.h"

RenderArea::RenderArea(QWidget *parent)
    : QWidget(parent)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    update();
}

QSize RenderArea::minimumSizeHint() const
{
    return QSize(400, 400);
}

class PlacedBoard : public QRectF
{
    BoardPtr board;

public:
    enum class Dir {vertical, horizontal};

    PlacedBoard(QPointF p, BoardPtr board, Dir dir)
        :QRectF(p, QSizeF(board->len, board->width))
        ,board(std::move(board))
        ,dir(dir)
    {
        if(dir == PlacedBoard::Dir::vertical)
        {
            auto vsize = size();
            vsize.transpose();
            auto y = p.y() - vsize.height();
            p.setY(y);
            setTopLeft(p);
            setSize(vsize);
        }
    }

    BoardPtr takeBoard()
    {
        return std::move(board);
    }

    void draw(QPainter& painter)
    {
        qDebug() << *static_cast<QRectF*>(this);
        painter.drawRect(*this);

        constexpr double dekorDist = 20;
        if(width() < 2*dekorDist || height() < 2*dekorDist)
            return;

        QPointF A(topLeft()+QPointF(dekorDist,dekorDist));
        QPointF B(topRight()-QPointF(dekorDist,-dekorDist));
        QPointF C(bottomRight()-QPointF(dekorDist,dekorDist));
        QPointF D(bottomLeft()-QPointF(-dekorDist,dekorDist));

        if(dir == PlacedBoard::Dir::vertical)
        {
            if(!board->cutH || !board->cutL){
                painter.save();
                QPen pen(painter.pen());
                pen.setStyle(Qt::PenStyle::DotLine);
                painter.setPen(pen);
                if(!board->cutH)
                    painter.drawLine(A,B);
                if(!board->cutL)
                    painter.drawLine(B,C);
                painter.restore();
            }
            if(!board->cutT || !board->cutR){
                painter.save();
                QPen pen(painter.pen());
                pen.setStyle(Qt::PenStyle::DashLine);
                painter.setPen(pen);
                if(!board->cutT)
                    painter.drawLine(C,D);
                if(!board->cutR)
                    painter.drawLine(D,A);
                painter.restore();
            }
        }
        else
        {
            if(!board->cutH || !board->cutL){
                painter.save();
                QPen pen(painter.pen());
                pen.setStyle(Qt::PenStyle::DotLine);
                painter.setPen(pen);
                if(!board->cutH)
                    painter.drawLine(B,C);
                if(!board->cutL)
                    painter.drawLine(C,D);
                painter.restore();
            }
            if(!board->cutT || !board->cutR){
                painter.save();
                QPen pen(painter.pen());
                pen.setStyle(Qt::PenStyle::DashLine);
                painter.setPen(pen);
                if(!board->cutT)
                    painter.drawLine(D,A);
                if(!board->cutR)
                    painter.drawLine(A,B);
                painter.restore();
            }
        }
    }

private:
    Dir dir;
};

struct Steny
{
    QRectF dvere;
    QRectF nosnaVonkajsia;
    QRectF nosnaVnutorna;
    QRectF prieckaStred;
    QRectF prieckaSused;
};

class Placer
{
    BoardFactory& boardFactory;
    PlacedBoard::Dir dir;
    qreal& (QPointF::*fw)() = &QPointF::rx;
    qreal& (QPointF::*side)() = &QPointF::ry;
    double factor = 1;

public:
    Placer(PlacedBoard::Dir dir, BoardFactory& boardFactory)
        :boardFactory(boardFactory)
        ,dir(dir)
    {
        if(dir == PlacedBoard::Dir::vertical)
        {
            fw = &QPointF::rx;
            side = &QPointF::ry;
        }
    }

    using PlacedBoards = std::vector<std::unique_ptr<PlacedBoard>>;

    PlacedBoards place(QPointF start,
                       const QRectF* block,
                       const QRectF* blockSide,
                       const QRectF* dvere=nullptr,
                       const QRectF* blockDvere=nullptr,
                       double firtsLineCut=0)
    {
        std::vector<std::unique_ptr<PlacedBoard>> rv;

        bool leftSideReached = false;
        bool firstLine = true;

        while(!leftSideReached)
        {
            QPointF lineStart = start;
            bool headSideReached = false;
            while(!headSideReached)
            {
                auto b = boardFactory.aquire();
                if(!b)
                    return rv;

                auto pb = new PlacedBoard(start, std::move(b), dir);
                qDebug() << *block << *pb;
                bool blocked1 = blockDvere && intersect(*blockDvere, *pb) && dvere && !intersect(*dvere, *pb);
                bool blocked = blocked1 || intersect(*block, *pb);
                auto tmpStart = start;
                if( blocked1 ||   blocked ){
                    double cutlen = 0;
                    if(blocked1)
                        cutlen = cut(*pb, *blockDvere);
                    else
                        cutlen = cut(*pb, *block);

                    auto b = pb->takeBoard();
                    auto bt = b->cutFw(cutlen);
                    pb = new PlacedBoard(start, std::move(bt), dir);
                    boardFactory.stackPush(std::move(b));
                    headSideReached = true;
                    start = lineStart + nextS(*pb);
                }
                else{
                    start += nextP(*pb);
                }

                /*if(firstLine && firtsLineCut > 1)
                {
                    qDebug() << "firtsLineCut" << *pb;
                    auto b = pb->takeBoard();
                    auto bside = b->cutLeftSide(firtsLineCut);
                    pb = new PlacedBoard(tmpStart, std::move(b), dir);
                }*/

                rv.emplace_back(pb);
            }

            if(rv.empty() || intersect(*blockSide, *rv.back()))
                    leftSideReached = true;

            firstLine = false;
        }

        return rv;
    }

private:

    QPointF nextP(const PlacedBoard& b) const
    {
        return dir == PlacedBoard::Dir::horizontal ?
            QPointF{b.width(), 0} :
            QPointF{0, -b.height()};
    }

    QPointF nextS(const PlacedBoard& b) const
    {
        return dir == PlacedBoard::Dir::horizontal ?
            QPointF{0, b.height()} :
            QPointF{b.width(), 0};
    }

    bool intersect(const QRectF& a, const QRectF& b) const
    {
        if(a.intersects(b)){
            auto irect = a.intersected(b);
            auto len = dir == PlacedBoard::Dir::horizontal ?
                        irect.width() : irect.height();
            return len > 0.1;
        }
        return false;
    }

    bool intersectSide(const QRectF& a, const QRectF& b) const
    {
        if(a.intersects(b)){
            auto irect = a.intersected(b);
            auto len = dir == PlacedBoard::Dir::horizontal ?
                        irect.height() : irect.width();
            return len > 0.1;
        }
        return false;
    }

    double cut(const QRectF& a, const QRectF& b) const
    {
        auto irect = a.intersected(b);
        if(dir == PlacedBoard::Dir::horizontal){
            return a.right()-irect.left();
        }
        else{
            return irect.bottom() - a.top();
        }
    }

    double cutSide(const QRectF& a, const QRectF& b) const
    {
        auto irect = a.intersected(b);
        return dir == PlacedBoard::Dir::horizontal ?
                        irect.height() : irect.width();
    }
};

void RenderArea::paintEvent(QPaintEvent * /* event */)
{
    constexpr double dilat = 10;
    constexpr double roomV = 5140-2*dilat;
    constexpr double room1H = 4330-2*dilat;
    constexpr double room2H = 4650-2*dilat;
    constexpr double wallWidth = 180+2*dilat;
    constexpr double roomsH = room1H + wallWidth + room2H;
    constexpr double doorOfset = 50+dilat;
    constexpr double doorWith = 900-2*dilat;

    QPainterPath path;
    path.moveTo(-wallWidth, -wallWidth);
    path.lineTo(roomsH+wallWidth, -wallWidth);
    path.lineTo(roomsH+wallWidth, roomV+wallWidth);
    path.lineTo(-wallWidth, roomV+wallWidth);
    path.closeSubpath();
    path.moveTo(0,0);
    path.lineTo(room1H,0);
    path.lineTo(room1H,doorOfset);
    path.lineTo(room1H+wallWidth, doorOfset);
    path.lineTo(room1H+wallWidth,0);
    path.lineTo(roomsH,0);
    path.lineTo(roomsH,roomV);
    path.lineTo(roomsH-room2H,roomV);
    path.lineTo(roomsH-room2H, doorWith+doorOfset);
    path.lineTo(room1H, doorWith+doorOfset);
    path.lineTo(room1H,roomV);
    path.lineTo(0,roomV);
    path.closeSubpath();

    const Steny stena {
        QRectF(QPoint(room1H,doorOfset), QSizeF(wallWidth,doorWith)),
        QRectF(QPoint(0,roomV), QSizeF(100e3,100e3)),
        QRectF(QPoint(0,-wallWidth), QSizeF(100e3,wallWidth)),
        QRectF (QPoint(room1H,0), QSizeF(wallWidth,100e3)),
        QRectF (QPoint(roomsH,0), QSizeF(wallWidth,100e3)),
    };

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(0.1,-0.1);
    painter.translate(wallWidth, -roomV-wallWidth);

    painter.setPen(QPen(Qt::green, 0, Qt::SolidLine,
                        Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(QBrush{Qt::green, Qt::BrushStyle::FDiagPattern});
    painter.drawPath(path);

    painter.setPen(QPen(Qt::cyan, 0, Qt::SolidLine,
                        Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(QBrush{Qt::cyan, Qt::BrushStyle::FDiagPattern});
    painter.drawRect(stena.dvere);

    BoardFactory boardFactory;
    std::unique_ptr<Placer> placer;
    Placer::PlacedBoards boards;

    painter.setPen(QPen(Qt::red, 0, Qt::SolidLine,
                        Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(QBrush{Qt::cyan, Qt::BrushStyle::NoBrush});

    placer = std::unique_ptr<Placer>(new Placer(PlacedBoard::Dir::vertical, boardFactory));
    boards = placer->place(QPoint(0,roomV),
                           &stena.nosnaVnutorna, &stena.prieckaSused,
                           nullptr, nullptr,
                           300);

    for(auto& a : boards)
    {
        a->draw(painter);
    }

    painter.setPen(QPen(Qt::blue, 0, Qt::SolidLine,
                        Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(QBrush{Qt::cyan, Qt::BrushStyle::NoBrush});

    placer = std::unique_ptr<Placer>(new Placer(PlacedBoard::Dir::horizontal, boardFactory));
    boards = placer->place(QPoint(0,0),
                                &stena.prieckaSused, &stena.nosnaVonkajsia,
                                &stena.dvere, &stena.prieckaStred);

    for(auto& a : boards)
    {
        a->draw(painter);
    }
}

