#pragma once

#include<memory>
#include<stack>

struct Board;

using BoardPtr = std::unique_ptr<Board>;

struct Board
{
  static constexpr double defLen = 2750;
  static constexpr double defWidth = 1200;
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
    static constexpr int count = 7;
    int left = count;
  public:
    BoardPtr aquire()
    {
        if(!stack.empty()){
            auto rv = std::move(stack.top());
            stack.pop();
            return rv;
        }

        if(left == count){
            left--;
            auto rv = new Board;
            rv->len -= 0;
            return BoardPtr(rv);
        }

        if(left--){
            return BoardPtr(new Board);
        }

        qCritical() << "no more boards";
        return nullptr;
    }

    void stackPush(BoardPtr&& board)
    {
        stack.push(std::move(board));
    }
};
