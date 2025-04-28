#include "User.h"

std::vector<User> User::allUsers;

User::User() : 
    loggedIn(false),
    batteryLevel(0),
    x(0),
    y(0),
    z(0),
    vezes(0),
    deviceTypeUser(0),
    timeActivity(0),
    frameType(""),
    bleuuid(""),
    analog(0, true, 0.8)
{
} 