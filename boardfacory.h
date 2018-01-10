#pragma once

#include<memory>
#include<stack>

struct Board;

using BoardPtr = std::unique_ptr<Board>;

struct Board
{
  static constexpr double defLen = 2050;
  static constexpr double defWidth = 625;
  double len = defLen;
  double width = defWidth;
  bool cutH = false;
  bool cutT = false;
  bool cutL = false;
  bool cutR = false;

  BoardPtr cutFw(double cutlen)
  {
      Q_ASSERT(cutlen > 0);
      auto tail = new Board;

      *tail = *this;
      tail->len = len - cutlen;
      tail->cutH = true;

      len = cutlen;
      cutT = true;

      return BoardPtr(tail);
  }

  BoardPtr cutLeftSide(double cutlen)
  {
      Q_ASSERT(cutlen > 0);
      auto tail = new Board;

      *tail = *this;
      tail->len = cutlen;
      tail->cutR = true;

      width = width-cutlen;
      cutL = true;

      return BoardPtr(tail);
  }
};

class BoardFactory
{
    std::stack<BoardPtr> stack;
    int count = 74;
  public:
    BoardPtr aquire()
    {
        if(!stack.empty()){
            auto rv = std::move(stack.top());
            stack.pop();
            qDebug() << "pop" << rv->len << rv->width;
            return rv;
        }

        if(count == 74){
            count--;
            auto rv = new Board;
            rv->len -= 1500;
            return BoardPtr(rv);
        }

        if(count--){
            qDebug() << "boards left" << count;
            return BoardPtr(new Board);
        }

        qCritical() << "no more boards";
        return nullptr;
    }

    void stackPush(BoardPtr&& board)
    {
        qDebug() << "push" << board->len << board->width;
        stack.push(std::move(board));
    }
};
