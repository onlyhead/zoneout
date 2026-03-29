#include "doctest/doctest.h"
#include "zoneout/zoneout.hpp"

namespace dp = datapod;
using namespace zoneout;

// Helper function to create a simple rectangular boundary
dp::Polygon create_test_boundary(double width = 100.0, double height = 50.0) {
    dp::Polygon rect;
    rect.vertices.push_back(dp::Point{0.0, 0.0, 0.0});
    rect.vertices.push_back(dp::Point{width, 0.0, 0.0});
    rect.vertices.push_back(dp::Point{width, height, 0.0});
    rect.vertices.push_back(dp::Point{0.0, height, 0.0});
    return rect;
}

// ========== ZoneBuilder Tests ==========

TEST_CASE("ZoneBuilder basic construction") {
    dp::Geo datum{52.0, 5.0, 0.0};
    auto boundary = create_test_boundary();

    SUBCASE("Build valid zone with required fields only") {
        auto zone = ZoneBuilder()
                        .with_name("test_zone")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .build();

        CHECK(zone.name() == "test_zone");
        CHECK(zone.type() == "agricultural");
        CHECK(zone.is_valid());
    }

    SUBCASE("Build zone with optional resolution") {
        auto zone = ZoneBuilder()
                        .with_name("test_zone")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .with_resolution(0.5)
                        .build();

        CHECK(zone.is_valid());
        // Resolution affects grid size
        auto info = zone.raster_info();
        CHECK(info.find("202x102") != std::string::npos); // 100/0.5 + padding
    }

    SUBCASE("Build zone with properties") {
        auto zone = ZoneBuilder()
                        .with_name("test_zone")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .with_property("crop", "wheat")
                        .with_property("season", "2024")
                        .build();

        CHECK(zone.property("crop").value_or("") == "wheat");
        CHECK(zone.property("season").value_or("") == "2024");
    }

    SUBCASE("Build zone with multiple properties at once") {
        std::unordered_map<std::string, std::string> props = {{"crop", "corn"}, {"irrigation", "drip"}};

        auto zone = ZoneBuilder()
                        .with_name("test_zone")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .with_properties(props)
                        .build();

        CHECK(zone.property("crop").value_or("") == "corn");
        CHECK(zone.property("irrigation").value_or("") == "drip");
    }
}

TEST_CASE("ZoneBuilder with features") {
    dp::Geo datum{52.0, 5.0, 0.0};
    auto boundary = create_test_boundary();

    SUBCASE("Build zone with polygon feature") {
        dp::Polygon obstacle;
        obstacle.vertices.push_back(dp::Point{20.0, 20.0, 0.0});
        obstacle.vertices.push_back(dp::Point{30.0, 20.0, 0.0});
        obstacle.vertices.push_back(dp::Point{30.0, 30.0, 0.0});
        obstacle.vertices.push_back(dp::Point{20.0, 30.0, 0.0});

        auto zone = ZoneBuilder()
                        .with_name("test_zone")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .with_polygon_element(obstacle, "tree", "obstacle")
                        .build();

        auto element_info = zone.element_info();
        CHECK(element_info.find("1 polygons") != std::string::npos);
    }

    SUBCASE("Build zone with multiple features") {
        dp::Polygon obstacle1;
        obstacle1.vertices.push_back(dp::Point{20.0, 20.0, 0.0});
        obstacle1.vertices.push_back(dp::Point{25.0, 20.0, 0.0});
        obstacle1.vertices.push_back(dp::Point{25.0, 25.0, 0.0});
        obstacle1.vertices.push_back(dp::Point{20.0, 25.0, 0.0});

        dp::Polygon obstacle2;
        obstacle2.vertices.push_back(dp::Point{40.0, 30.0, 0.0});
        obstacle2.vertices.push_back(dp::Point{45.0, 30.0, 0.0});
        obstacle2.vertices.push_back(dp::Point{45.0, 35.0, 0.0});
        obstacle2.vertices.push_back(dp::Point{40.0, 35.0, 0.0});

        auto zone = ZoneBuilder()
                        .with_name("test_zone")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .with_polygon_element(obstacle1, "tree1", "obstacle")
                        .with_polygon_element(obstacle2, "tree2", "obstacle")
                        .build();

        auto element_info = zone.element_info();
        CHECK(element_info.find("2 polygons") != std::string::npos);
    }
}

TEST_CASE("ZoneBuilder validation") {
    dp::Geo datum{52.0, 5.0, 0.0};
    auto boundary = create_test_boundary();

    SUBCASE("Missing name fails validation") {
        ZoneBuilder builder;
        builder.with_type("agricultural").with_boundary(boundary).with_datum(datum);

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("name") != std::string::npos);
        CHECK_THROWS_AS(builder.build(), std::invalid_argument);
    }

    SUBCASE("Missing type fails validation") {
        ZoneBuilder builder;
        builder.with_name("test").with_boundary(boundary).with_datum(datum);

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("type") != std::string::npos);
        CHECK_THROWS_AS(builder.build(), std::invalid_argument);
    }

    SUBCASE("Missing boundary fails validation") {
        ZoneBuilder builder;
        builder.with_name("test").with_type("agricultural").with_datum(datum);

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("boundary") != std::string::npos);
        CHECK_THROWS_AS(builder.build(), std::invalid_argument);
    }

    SUBCASE("Missing datum fails validation") {
        ZoneBuilder builder;
        builder.with_name("test").with_type("agricultural").with_boundary(boundary);

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("datum") != std::string::npos);
        CHECK_THROWS_AS(builder.build(), std::invalid_argument);
    }

    SUBCASE("Invalid resolution fails validation") {
        ZoneBuilder builder;
        builder.with_name("test")
            .with_type("agricultural")
            .with_boundary(boundary)
            .with_datum(datum)
            .with_resolution(-1.0);

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("Resolution") != std::string::npos);
        CHECK_THROWS_AS(builder.build(), std::invalid_argument);
    }

    SUBCASE("Boundary with too few points fails validation") {
        dp::Polygon invalid_boundary;
        invalid_boundary.vertices.push_back(dp::Point{0.0, 0.0, 0.0});
        invalid_boundary.vertices.push_back(dp::Point{10.0, 0.0, 0.0});

        ZoneBuilder builder;
        builder.with_name("test").with_type("agricultural").with_boundary(invalid_boundary).with_datum(datum);

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("at least 3 points") != std::string::npos);
    }
}

TEST_CASE("ZoneBuilder reset functionality") {
    dp::Geo datum{52.0, 5.0, 0.0};
    auto boundary = create_test_boundary();

    ZoneBuilder builder;

    SUBCASE("Reset clears all configuration") {
        builder.with_name("test1")
            .with_type("agricultural")
            .with_boundary(boundary)
            .with_datum(datum)
            .with_property("key", "value");

        builder.reset();

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("name") != std::string::npos);
    }

    SUBCASE("Builder can be reused after reset") {
        // First build
        auto zone1 =
            builder.with_name("zone1").with_type("agricultural").with_boundary(boundary).with_datum(datum).build();

        CHECK(zone1.name() == "zone1");

        // Reset and build again
        builder.reset();
        auto zone2 = builder.with_name("zone2").with_type("pasture").with_boundary(boundary).with_datum(datum).build();

        CHECK(zone2.name() == "zone2");
        CHECK(zone2.type() == "pasture");
    }
}

// ========== PlotBuilder Tests ==========

TEST_CASE("PlotBuilder basic construction") {
    dp::Geo datum{52.0, 5.0, 0.0};
    auto boundary = create_test_boundary();

    SUBCASE("Build valid plot with required fields only") {
        auto plot = PlotBuilder()
                        .with_name("test_plot")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .build();

        CHECK(plot.name() == "test_plot");
        CHECK(plot.type() == "agricultural");
        CHECK(plot.is_valid());
        CHECK_FALSE(plot.has_grid());
        CHECK(plot.poly().has_field_boundary());
    }

    SUBCASE("Build plot with properties") {
        auto plot = PlotBuilder()
                        .with_name("test_plot")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .with_property("owner", "Test Farm")
                        .with_property("location", "Netherlands")
                        .build();

        CHECK(plot.property("owner").value_or("") == "Test Farm");
        CHECK(plot.property("location").value_or("") == "Netherlands");
    }
}

TEST_CASE("PlotBuilder grid options") {
    dp::Geo datum{52.0, 5.0, 0.0};
    auto boundary = create_test_boundary();

    SUBCASE("Build plot with generated grid from resolution") {
        auto plot = PlotBuilder()
                        .with_name("grid_plot")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .with_resolution(1.0)
                        .build();

        CHECK(plot.has_grid());
        CHECK(plot.grid().layer_count() == 1);
    }

    SUBCASE("Build plot with provided initial grid") {
        dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Quaternion{}};
        dp::Grid<uint8_t> base_grid;
        base_grid.rows = 10;
        base_grid.cols = 10;
        base_grid.resolution = 1.0;
        base_grid.centered = true;
        base_grid.pose = shift;
        base_grid.data.resize(100);

        auto plot = PlotBuilder()
                        .with_name("grid_plot")
                        .with_type("agricultural")
                        .with_boundary(boundary)
                        .with_datum(datum)
                        .with_initial_grid(base_grid)
                        .build();

        CHECK(plot.has_grid());
        CHECK(plot.grid().layer_count() == 1);
    }
}

TEST_CASE("PlotBuilder validation") {
    dp::Geo datum{52.0, 5.0, 0.0};

    SUBCASE("Missing name fails validation") {
        PlotBuilder builder;
        builder.with_type("agricultural").with_boundary(create_test_boundary()).with_datum(datum);

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("name") != std::string::npos);
        CHECK_THROWS_AS(builder.build(), std::invalid_argument);
    }

    SUBCASE("Missing type fails validation") {
        PlotBuilder builder;
        builder.with_name("test").with_boundary(create_test_boundary()).with_datum(datum);

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("type") != std::string::npos);
        CHECK_THROWS_AS(builder.build(), std::invalid_argument);
    }

    SUBCASE("Missing datum fails validation") {
        PlotBuilder builder;
        builder.with_name("test").with_type("agricultural").with_boundary(create_test_boundary());

        CHECK_FALSE(builder.is_valid());
        CHECK(builder.validation_error().find("datum") != std::string::npos);
        CHECK_THROWS_AS(builder.build(), std::invalid_argument);
    }
}

TEST_CASE("PlotBuilder reset functionality") {
    dp::Geo datum{52.0, 5.0, 0.0};

    PlotBuilder builder;

    SUBCASE("Reset clears all configuration") {
        builder.with_name("test1")
            .with_type("agricultural")
            .with_boundary(create_test_boundary())
            .with_datum(datum)
            .with_property("key", "value");

        builder.reset();

        CHECK_FALSE(builder.is_valid());
    }

    SUBCASE("Builder can be reused after reset") {
        auto plot1 = builder.with_name("plot1")
                         .with_type("agricultural")
                         .with_boundary(create_test_boundary())
                         .with_datum(datum)
                         .build();

        CHECK(plot1.name() == "plot1");

        builder.reset();
        auto plot2 =
            builder.with_name("plot2").with_type("research").with_boundary(create_test_boundary()).with_datum(datum).build();

        CHECK(plot2.name() == "plot2");
        CHECK(plot2.type() == "research");
    }
}

TEST_CASE("PlotBuilder reset and rebuild with different boundaries") {
    dp::Geo datum{52.0, 5.0, 0.0};
    auto boundary1 = create_test_boundary(100.0, 50.0);
    auto boundary2 = create_test_boundary(60.0, 60.0);

    PlotBuilder builder;
    auto plot1 = builder.with_name("test1").with_type("agricultural").with_boundary(boundary1).with_datum(datum).build();
    CHECK(plot1.poly().area() == doctest::Approx(boundary1.area()));

    builder.reset();
    auto plot2 = builder.with_name("test2").with_type("research").with_boundary(boundary2).with_datum(datum).build();
    CHECK(plot2.poly().area() == doctest::Approx(boundary2.area()));
}
