#include "core/yaml_parser.h"
#include "core/tt_test.h"
#include "core/tt_log.h"


JUST_RUN_TEST(yaml, test);
TEST(yaml, test) 
{
    LOG_TEST("yaml test");

    YamlParser parser("/mnt/d/ws/test/yaml/data/test_simple.yaml");

    parser.PrintNodes();

    auto value = parser.GetNodeValue({"database", "host"});
    LOG_TEST("database host: ", value);

    value = parser.GetNodeValue({"database", "port"});
    LOG_TEST("database port: ", value);

    parser.SetNodeValue({"database", "port"}, "8080");
    parser.PrintNodes();

    LOG_TEST("========================================");

    parser.SetNodeValue({"database", "p"}, "22");
    parser.PrintNodes();
}
