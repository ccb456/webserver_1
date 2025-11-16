#include "server/webserver.h"

int main()
{
    Webserver server(9090, 3, 60000, false);
    server.run();
}