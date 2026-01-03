#include "board.h"
#include "cli.h"

int main()
{
    initBoard();
    cliInit();
    
    while (true)
    {
        cliProcess();
    }
}