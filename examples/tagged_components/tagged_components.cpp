#include <ecs/ecs.h>
#include <iostream>
#include <string>


struct flameable_t {
    ecs_flags(ecs::tag);
};
struct freezeable_t {
    ecs_flags(ecs::tag);
};
struct shockable_t {
    ecs_flags(ecs::tag);
};

struct Name : std::string {};

int main() {
    // Add systems for each tag. Will just print the name.
    // Since tags never hold any data, they are always passed by value
    ecs::make_system<0>([](Name const& name, freezeable_t) { std::cout << "  freezeable: " << name << "\n"; });
    ecs::make_system<1>([](Name const& name, shockable_t) { std::cout << "  shockable:  " << name << "\n"; });
    ecs::make_system<2>([](Name const& name, flameable_t) { std::cout << "  flammeable: " << name << "\n"; });

    // Set up the entities with a name and tags
    ecs::entity_id const jon{0}, sean{1}, jimmy{2}, rachel{3}, suzy{4};
    ecs::add_components(jon, Name{"Jon"}, freezeable_t{});
    ecs::add_components(sean, Name{"Sean"}, flameable_t{});
    ecs::add_components(jimmy, Name{"Jimmy"}, shockable_t{});
    ecs::add_components(rachel, Name{"Rachel"}, flameable_t{}, freezeable_t{}, shockable_t{});
    ecs::add_components(suzy, Name{"Suzy"}, flameable_t{});

    // Commit the changes
    ecs::commit_changes();
    std::cout << "Created 'Jon' with freezeable_t\n"
                 "        'Sean' with flameable_t\n"
                 "        'Jimmy' with shockable_t\n"
                 "        'Rachel' with flameable_t, freezeable_t, shockable_t\n"
                 "        'Suzy' with flameable_t\n\n";

    // Run the systems
    std::cout << "Running systems:\n";
    ecs::run_systems();

    std::cout << "\nStat dump:\n";
    std::cout << "  Number of entities with the flameable_t tag:  " << ecs::get_entity_count<flameable_t>()
              << "\n  Number of entities with the shockable_t tag:  " << ecs::get_entity_count<shockable_t>()
              << "\n  Number of entities with the freezeable_t tag: " << ecs::get_entity_count<freezeable_t>()
              << "\n  Number of flameable_t components allocated:   " << ecs::get_component_count<flameable_t>()
              << "\n  Number of shockable_t components allocated:   " << ecs::get_component_count<shockable_t>()
              << "\n  Number of freezeable_t components allocated:  " << ecs::get_component_count<freezeable_t>()
              << "\n";
}
