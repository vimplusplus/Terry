#include "boot_sequence.h"

namespace terry {

const std::string& atuin_art() {
    // Great A'Tuin — the Giant Star Turtle who carries the Disc
    static const std::string art = R"(
                          ___
                    _,-'`   `'-._
                 ,-'    A'TUIN    '-.
               ,'  .---.       .--.  '.
              /  /       \   /      \  \
             |  | O     O | | O    O |  |
              \  \  ~~~  / | \  ~~  /  /
               '.  '---'  |  '----'  .'
                 '-.___,--'---.___,-'
                     |  [===]  |
                  ___| [=====] |___
                 /   |_[=====]_|   \
                /    elephants go    \
               '~~~~~~~~~~~~~~~~~~~~~~~~~')";
    return art;
}

const std::vector<BootLine>& boot_script() {
    static const std::vector<BootLine> script = {
        { 180, "TERRY v1.0 — A DISCWORLD TERMINAL" },
        { 220, "" },
        { 280, "INITIATING TERRY SUBSYSTEMS..." },
        { 350, "CALIBRATING THAUMIC RESONANCE.......... NOMINAL" },
        { 300, "CONSULTING THE LIBRARIAN................. OOK" },
        { 320, "CHECKING TURTLE STATUS................... SWIMMING" },
        { 300, "VERIFYING ELEPHANT STRUCTURAL INTEGRITY.. 4/4" },
        { 280, "LOADING UNSEEN UNIVERSITY PROTOCOLS...... DONE" },
        { 220, "ACTIVATING OCTARINE SUBSYSTEMS........... ACTIVE" },
        { 180, "ENGAGING L-SPACE CONNECTIVITY............ STABLE" },
        { 160, "" },
        { 160, "WARNING: MAGIC LEVELS ABOVE NORMAL" },
        { 140, "WARNING: THIS IS FINE" },
        { 140, "" },
        { 300, "DEATH: GOOD MORNING. OR EVENING.",             true },
        { 250, "       I FIND THE DISTINCTION INCREASINGLY",   true },
        { 200, "       ACADEMIC.",                             true },
        { 200, "" },
        { 150, "       THE SHELL AWAITS.",                     true },
    };
    return script;
}

} // namespace terry
