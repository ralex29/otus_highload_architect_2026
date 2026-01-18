#include <greeting.hpp>

#include <userver/utest/utest.hpp>

using social_net_service::UserType;

UTEST(SayHelloTo, Basic) {
    EXPECT_EQ(social_net_service::SayHelloTo("Developer", UserType::kFirstTime), "Hello, Developer!\n");
    EXPECT_EQ(social_net_service::SayHelloTo({}, UserType::kFirstTime), "Hello, unknown user!\n");

    EXPECT_EQ(social_net_service::SayHelloTo("Developer", UserType::kKnown), "Hi again, Developer!\n");
    EXPECT_EQ(social_net_service::SayHelloTo({}, UserType::kKnown), "Hi again, unknown user!\n");
}