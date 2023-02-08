// Pick your poison.
//
// On GNU/Linux, you have few choices to get the most out of your stack trace.
//
// By default you get:
//	- object filename
//	- function name
//
// In order to add:
//	- source filename
//	- line and column numbers
//	- source code snippet (assuming the file is accessible)

// Install one of the following libraries then uncomment one of the macro (or
// better, add the detection of the lib and the macro definition in your build
// system)

// - apt-get install libdw-dev ...
// - g++/clang++ -ldw ...
// #define BACKWARD_HAS_DW 1

// - apt-get install binutils-dev ...
// - g++/clang++ -lbfd ...
// #define BACKWARD_HAS_BFD 1

// - apt-get install libdwarf-dev ...
// - g++/clang++ -ldwarf ...
// #define BACKWARD_HAS_DWARF 1

// Regardless of the library you choose to read the debug information,
// for potentially more detailed stack traces you can use libunwind
// - apt-get install libunwind-dev
// - g++/clang++ -lunwind
// #define BACKWARD_HAS_LIBUNWIND 1

#include "backward.hpp"

#include <crogine/core/App.hpp>

#include <fstream>

namespace backward {

    //void SignalHandling::handle_stacktrace(int skip_frames)
    //{
    //    // printer creates the TraceResolver, which can supply us a machine type
    //    // for stack walking. Without this, StackTrace can only guess using some
    //    // macros.
    //    // StackTrace also requires that the PDBs are already loaded, which is done
    //    // in the constructor of TraceResolver
    //    Printer printer;

    //    StackTrace st;
    //    st.set_machine_type(printer.resolver().machine_type());
    //    st.set_thread_handle(thread_handle());
    //    st.load_here(32 + skip_frames, ctx());
    //    st.skip_n_firsts(skip_frames);

    //    printer.address = true;
    //    printer.print(st, std::cerr);

    //    std::fstream outfile(cro::App::getPreferencePath() + "stacktrace.txt");
    //    if (outfile.good() && outfile.is_open())
    //    {
    //        printer.print(st, outfile);
    //    }
    //}

backward::SignalHandling sh;

} // namespace backward
