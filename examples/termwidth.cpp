#include <morph/tools.h>

int main()
{
    int twidth = morph::Tools::getTerminalWidth();
    if (twidth >= 0) {
        std::cout << "Your terminal width is " << twidth << " columns. Congratulations!\n";
    } else {
        std::cout << "The getTerminalWidth() call failed. Commiserations.\n";
    }
    return twidth;
}
