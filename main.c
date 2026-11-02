#include <syslog.h>

#define PROGRAM_NAME "temperature-controller"

int main(void)
{
    openlog(PROGRAM_NAME, 0, LOG_USER);

    syslog(LOG_INFO, "Program has started");

    return 0;
}
