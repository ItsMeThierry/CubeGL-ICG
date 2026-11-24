#include "app.h"

int main() {
    ClientApp app;
    if (!app.init("config.json")) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
