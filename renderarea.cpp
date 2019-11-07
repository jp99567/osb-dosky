
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
        painter.drawRect(*this);

        constexpr double dekorDist = 20;
        if(width() < 2*dekorDist || height() < 2*dekorDist){
            qDebug() << "uzky obdlznik " << *static_cast<const QRectF*>(this);
            return;
        }

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

class Placer
{
    BoardFactory& boardFactory;
    PlacedBoard::Dir dir;

public:
    Placer(PlacedBoard::Dir dir, BoardFactory& boardFactory)
        :boardFactory(boardFactory)
        ,dir(dir)
    {
    }

    using PlacedBoards = std::vector<std::unique_ptr<PlacedBoard>>;

    PlacedBoards place(QPointF start,
                       const QRectF& block,
                       const QRectF& blockSide,
                       double firtsLineCut=0)
    {
        std::vector<std::unique_ptr<PlacedBoard>> rv;

        bool leftSideReached = false;
        bool firstLine = true;

        int riadok=1;

        while(!leftSideReached)
        {
            QPointF lineStart = start;
            bool headSideReached = false;
            int cislo = 1;
            while(!headSideReached)
            {
                auto b = boardFactory.aquire();
                if(!b){
                    qCritical() << "nie su dosky";
                    return rv;
                }

                auto pb = new PlacedBoard(start, std::move(b), dir);
                bool blocked = intersect(block, *pb);
                auto tmpStart = start;
                if( blocked ){
                    double cutlen = cut(*pb, block);
                    auto b = pb->takeBoard();
                    auto bt = b->cutFw(cutlen);
                    pb = new PlacedBoard(start, std::move(bt), dir);
                    boardFactory.stackPush(std::move(b));
                    headSideReached = true;
                    start = lineStart + nextS(*pb);
                    if(firstLine && firtsLineCut > 0){
                        start -= QPointF(firtsLineCut,0);
                    }
                }
                else{
                    start += nextP(*pb);
                }

                if(firstLine && firtsLineCut > 0)
                {
                    auto b = pb->takeBoard();
                    auto bside = b->cutLeftSide(firtsLineCut);
                    pb = new PlacedBoard(tmpStart, std::move(b), dir);
                }

                qInfo() << riadok << cislo << *static_cast<const QRectF*>(pb);
                rv.emplace_back(pb);
                ++cislo;
            }

            if(rv.empty() || intersectSide(blockSide, *rv.back())){
                    leftSideReached = true;
            }

            firstLine = false;
            ++riadok;
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
    constexpr double roomV = 5140;
    constexpr double roomH = 4600; //4600
    constexpr double wallWidth = 200;

    QPainterPath path;
    path.moveTo(-wallWidth, -wallWidth);
    path.lineTo(roomH+wallWidth, -wallWidth);
    path.lineTo(roomH+wallWidth, roomV+wallWidth);
    path.lineTo(-wallWidth, roomV+wallWidth);
    path.closeSubpath();

    path.moveTo(0,0);
    path.lineTo(roomH,0);
    path.lineTo(roomH,roomV);
    path.lineTo(0, roomV);
    path.closeSubpath();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(0.1,-0.1);
    painter.translate(wallWidth, -roomV-wallWidth);

    painter.setPen(QPen(Qt::green, 0, Qt::SolidLine,
                        Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(QBrush{Qt::green, Qt::BrushStyle::FDiagPattern});
    painter.drawPath(path);

    {
        painter.setPen(QPen(Qt::gray, 0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.setBrush(QBrush{Qt::gray, Qt::BrushStyle::DiagCrossPattern});
        painter.drawRect(QRectF(QPointF(0,0), QSizeF(90, roomV)));

        QPointF trampos(810,0);
        int tramNr = 1;
        while( trampos.x() < roomH-180)
        {
            qDebug() << "tram" << tramNr++ << trampos;
            QRectF tram(trampos, QSizeF(180, roomV));
            painter.drawRect(tram);
            trampos.rx() += 910;
        }
        painter.drawRect(QRectF(QPointF(roomH-90,0), QSizeF(90, roomV)));
    }

    {
        painter.setPen(QPen(Qt::black, 0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.setBrush(QBrush{Qt::black, Qt::BrushStyle::SolidPattern});
        constexpr double michnoR = 60.0;

        auto placeMichno = [&painter](QPointF p){
          p -= QPointF(michnoR, michnoR);
          painter.drawEllipse(QRectF(p, QSizeF(2*michnoR,2*michnoR)));
        };

        placeMichno(QPointF(michnoR,michnoR));
        QPointF p(michnoR, Board::defWidth / 3);
        auto michnoNr = 0;
        while( p.y() < roomV-michnoR) {
            qDebug() << "michno" << michnoNr++ << p;
            placeMichno(p);
            p.ry() += Board::defWidth / 3;
        }
        placeMichno(QPointF(michnoR,roomV-michnoR));
    }

    BoardFactory boardFactory;
    std::unique_ptr<Placer> placer;
    Placer::PlacedBoards boards;

    painter.setPen(QPen(Qt::red, 0, Qt::SolidLine,
                        Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(QBrush{Qt::cyan, Qt::BrushStyle::NoBrush});

    qDebug() << "SPODNA";
    placer = std::unique_ptr<Placer>(new Placer(PlacedBoard::Dir::horizontal, boardFactory));
    boards = placer->place(QPointF(0,0),
                           QRectF(QPointF(roomH,0), QSizeF(wallWidth,100e3)),
                           QRectF(QPointF(0,0), QSizeF(-wallWidth,100e3)), 0);

    for(auto& a : boards)
    {
        a->draw(painter);
    }


}

