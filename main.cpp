#include "server/webserver.h"

int main()
{
    Webserver server(8080, 3, 60000, false);
    server.run();
}