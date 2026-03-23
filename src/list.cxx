#include <iostream>

#include <package.hxx>
#include <spkg.hxx>

int spkg::List(const Config &config)
{
    ForEachPackage(
        config,
        [](Package &&pkg)
        {
            std::cout << pkg.Id << std::endl;
            std::cout << pkg.Name << " - " << pkg.Description << std::endl;

            for (auto &[key, fragment] : pkg.Fragments)
            {
                std::cout << "   - " << key << std::endl;
                std::cout << "     " << fragment.Name << " - " << fragment.Description << std::endl;
            }

            return false;
        });

    return 0;
}
