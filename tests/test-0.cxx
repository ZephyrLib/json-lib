
#include <cassert>
#include <sstream>

#include "json-lib/json.hpp"

int main(int, char **) {

    std::stringstream ss{ R"(
{
    "a": 1,
    "b": 2,
    "c": {
            "d": 3
        },
    "e": null,
    "f": [],
    "g": [
        4, 8, 16, 32, false
    ],
    "h": [
        5,
        null
    ],
    "i" : "J",
    "k": 345.7,
    "l": "nYa\nuwu"
}
)" };

    const auto j = json::parse(ss);

    assert(j->asObject().at("a")->asPrimitive().number() == 1);
    assert(j->asObject().at("b")->asPrimitive().number() == 2);
    assert(j->asObject().at("c")->asObject().at("d")->asPrimitive().number() == 3);
    assert(j->asObject().at("e") == nullptr);
    assert(j->asObject().at("f")->asArray().size() == 0);
    assert(j->asObject().at("g")->asArray().at(0)->asPrimitive().number() == 4);
    assert(j->asObject().at("h")->asArray().at(1)->asPrimitive().isNull());
    assert(j->asObject().at("i")->asPrimitive().literal() == "J");
    assert(*j->asObject().at("k") == json::P(345.7));

    auto k = json::O();
    {
        using namespace json;

        k.set("a", new P(1));
        k.set("b", new P(2));
        k.set("c", &(new O())->set("d", new P(3)));
        k.set("e", new P());
        k.set("f", new A());
        k.set("g", &(new A())
            ->add(new P(4))
            .add(new P(8))
            .add(new P(16))
            .add(new P(32))
            .add(new P(False))
        );
        k.set("h", &(new A())
            ->add(new P(5))
            .add(nullptr)
        );
        k.set("i", new P("J"));
        k.set("k", new P(345.7));
        k.set("l", new P("nYa\nuwu"));
    }

    assert(*j == k);

}
