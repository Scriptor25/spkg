#include <iostream>

#include <spkg.hxx>

int spkg::Help()
{
    std::cout <<
            "spkg\n"
            "\n"
            "spkg [help|h]                       - print manual\n"
            "spkg (list|l)                       - list all packages\n"
            "spkg (install|i) <id>[:<fragment>]  - install package\n"
            "spkg (remove|r) <id>[:<fragment>]   - remove package\n"
            "spkg (update|u) [<id>[:<fragment>]] - update all or specific package\n"
            << std::endl;

    return 0;
}
