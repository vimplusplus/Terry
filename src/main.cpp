#include "config.h"
#include "tui_app.h"
#include <cstdlib>

int main(int argc, char* argv[]) {
    terry::Config cfg = terry::load_config();

    // Allow override from command line: terry /path/to/shell
    if (argc > 1) cfg.shell_path = argv[1];

    terry::TuiApp app(std::move(cfg));
    return app.Run();
}
