/* Author : Vishvender Malik
Email : vishvenderm@iiitd.ac.in
File : function_definitions.cpp
*/

#include "function_definitions.h"
#include "macros.h"

namespace function{

    // function to receive Desired airspeed (published on to the topic) from the topic itself
    void get_guidance_controller_velocity(const geometry_msgs::TwistStamped::ConstPtr& data)
    {
        array_velocity_guidance[0] = data -> twist.linear.x;
        array_velocity_guidance[1] = data -> twist.linear.y;
        array_velocity_guidance[2] = data -> twist.linear.z;
        ROS_INFO("Data received from topic \"/mavros/local_position/velocity\".");
    }

    void get_global_position_uav(const sensor_msgs::NavSatFix::ConstPtr& data)
    {
        if(home_init){
            location_home_lat_x = data -> latitude;
            location_home_long_y = data -> longitude;
            location_home_alt_z = data -> altitude;
            home_init = false;
        }

        array_global_position_uav[0] = data -> latitude;
        array_global_position_uav[1] = data -> longitude;
        array_global_position_uav[2] = data -> altitude;

        // convert current waypoint's lat long coordinates to x y coordinates
        convert_lat_long_to_x_y(location_home_lat_x, location_home_long_y, array_global_position_uav[0],
        array_global_position_uav[1], array_wp, array_wp_size);
        // assuming home location to be 0, 0

        array_local_position_pose_data[0] = wp_x;
        array_local_position_pose_data[1] = wp_y;
        //array_local_position_pose_data[2] = data -> altitude;

        //ROS_INFO("Data received from topic \"mavros/global_position/global\".\n");
        /*
        ROS_INFO("\nlocation_home_lat_x : %f\n"
        "location_home_long_y : %f\n"
        "location_home_alt_z : %f\n"
        "UAV lat from global x : %f\n"
        "UAV long from global y : %f\n"
        "UAV position x : %f\n"
        "UAV position y : %f\n\n", 
        location_home_lat_x, location_home_long_y, location_home_alt_z, 
        array_global_position_uav[0], array_global_position_uav[1],
        array_local_position_pose_data[0], array_local_position_pose_data[1]);
        */
    }

    // function to receive mission waypoints
    void get_waypoint_list(const mavros_msgs::WaypointList::ConstPtr& list)
    {
        waypoint_current = list -> current_seq;

        if(list_trigger){
            size_waypoint_list = list -> waypoints.size();

            for(int i = 0; i < size_waypoint_list; i++){
                array_waypoint_list[i].x_lat = list -> waypoints[i].x_lat;
                array_waypoint_list[i].y_long = list -> waypoints[i].y_long;
                array_waypoint_list[i].z_alt = list -> waypoints[i].z_alt;
            }
            list_trigger = false;
        }
        // convert current waypoint's lat long coordinates to x y coordinates
        convert_lat_long_to_x_y(location_home_lat_x, location_home_long_y, array_waypoint_list[waypoint_current].x_lat,
        array_waypoint_list[waypoint_current].y_long, array_wp, array_wp_size);
        // assuming home location to be 0, 0
        ROS_INFO("Data received from topic \"/mavros/mission/waypoints\".");
        ROS_INFO("No. of waypoints received : %d\n"
        "Current waypoint : %d\n\n"
        "wp_x : %f\n""wp_y : %f\n",
        size_waypoint_list, waypoint_current, wp_x, wp_y);
    }

    // function to calculate equivalent x, y points from lat long coordinates of a wp
    double convert_lat_long_to_x_y(double x_lat_home, double y_long_home, double x_lat_mission_wp, 
    double y_long_mission_wp, double array_wp[], const int array_wp_size)
    {
        diff_in_lat = x_lat_mission_wp - x_lat_home;
        diff_in_long = y_long_mission_wp - y_long_home;

        some_parameter_a = (sin(diff_in_lat / 2.0) * sin(diff_in_lat / 2.0)) + (cos(x_lat_home) * cos(x_lat_mission_wp) * 
                            (sin(diff_in_long / 2.0) * sin(diff_in_long / 2.0)));
        some_parameter_c = 2 * atan2(sqrt(some_parameter_a), sqrt(1 - some_parameter_a));
        some_parameter_d = m_radius_of_earth * some_parameter_c * 1000;
        some_parameter_y = sin(diff_in_long) * cos(x_lat_home);
        some_parameter_x = (sin(x_lat_home) * cos(x_lat_mission_wp) * cos(diff_in_long)) - 
                            (cos(x_lat_home) * sin(x_lat_mission_wp));
        some_parameter_x = - some_parameter_x;
        some_parameter_bearing = fmod((atan2(some_parameter_y, some_parameter_x) + (2 * M_PI)), (2 * M_PI));

        wp_x = (some_parameter_d * cos(some_parameter_bearing));
        wp_y = (some_parameter_d * sin(some_parameter_bearing));

        array_wp[0] = (some_parameter_d * cos(some_parameter_bearing));
        array_wp[1] = (some_parameter_d * sin(some_parameter_bearing));

        return *array_wp;
    }

    // function to receive mission waypoints
    void get_waypoint_list_plane(const mavros_msgs::WaypointList::ConstPtr& list)
    {
        if(waypoint_current != list -> current_seq){
            waypoint_old = waypoint_current;
            waypoint_new = list -> current_seq;
            waypoint_current = waypoint_new;
        }
        // to store list just one time
        if(list_trigger){
            waypoint_old = waypoint_current;
            size_waypoint_list = list -> waypoints.size();

            for(int i = 0; i < size_waypoint_list; i++){
                // populate array to store wp list
                array_waypoint_list[i].x_lat = list -> waypoints[i].x_lat;
                array_waypoint_list[i].y_long = list -> waypoints[i].y_long;
                array_waypoint_list[i].z_alt = list -> waypoints[i].z_alt;
                
                // populate a waypoint message to be put into table
                message_waypoint.frame = list -> waypoints[i].frame;
                message_waypoint.command = list -> waypoints[i].command; // uint16 NAV_WAYPOINT = 16, # Navigate to waypoint
                message_waypoint.is_current = list -> waypoints[i].is_current;
                message_waypoint.autocontinue = list -> waypoints[i].autocontinue;
                message_waypoint.param1 = 0.0; // No. of turns by UAV at wp
                message_waypoint.param2 = 0.0;
                message_waypoint.param3 = 0.0;
                message_waypoint.param4 = 0.0;
                message_waypoint.x_lat = list -> waypoints[i].x_lat;
                message_waypoint.y_long = list -> waypoints[i].y_long;
                message_waypoint.z_alt = list -> waypoints[i].z_alt;
                /*
                // populate vector to create editable wp table
                if(i == 0){
                    //vec_waypoint_table.insert(vec_waypoint_table.begin(), message_waypoint);
                }else{
                    vec_waypoint_table.push_back(message_waypoint);
                }*/
                //vec_waypoint_table.push_back(message_waypoint);
                array_waypoints_plane[i] = message_waypoint;
            }
            list_trigger = false;
        }
        // convert current waypoint's lat long coordinates to x y coordinates
        convert_lat_long_to_x_y(location_home_lat_x, location_home_long_y, array_waypoint_list[waypoint_current].x_lat,
        array_waypoint_list[waypoint_current].y_long, array_wp, array_wp_size);
        // assuming home location to be 0, 0
        ROS_INFO("Data received from topic \"/mavros/mission/waypoints\".");
        ROS_INFO("No. of waypoints received : %d\n"
        "Old waypoint : %d\n"
        "New waypoint : %d\n"
        "Current waypoint : %d\n\n"
        "wp_x : %f\n""wp_y : %f\n",
        size_waypoint_list, waypoint_old, waypoint_new, waypoint_current, wp_x, wp_y);
    }

    // function to calculate variable bearing for use in function xy_2latlon
    double find_bearing(double wp_x, double wp_y)
    {
        bearing = atan2(wp_y, wp_x);
        //std::cout<<"bearing : "<<bearing<<"\n\n";
        return bearing;
    }

    // function to convert simple x y coordinates of a waypoint to lat long coordinates
    void xy_2latlon(double x_lat_home, double y_long_home, int wp_x, int wp_y, double bearing)
    {
        x_lat_home = x_lat_home * (m_pi / 180); // to radians
        y_long_home = y_long_home * (m_pi / 180);

        std::cout<<"x_lat_home : "<<x_lat_home<<"\n""y_long_home : "<<y_long_home<<"\n";        
        
        some_parameter_d = sqrt(pow(wp_x, 2) + pow(wp_y, 2));
        x_to_lat = asin(sin(x_lat_home) * cos(some_parameter_d / m_radius_earth) + cos(x_lat_home) * sin(some_parameter_d / m_radius_earth)
                    * cos(find_bearing(wp_x, wp_y)));
        y_to_long = y_long_home + atan2(sin(find_bearing(wp_x, wp_y)) * sin(some_parameter_d / m_radius_earth)
                    *cos(x_lat_home), cos(some_parameter_d / m_radius_earth) - sin(x_lat_home) * sin(x_to_lat));

        x_to_lat = abs(x_to_lat * (180 / m_pi)); // to degrees
        y_to_long = abs(y_to_long * (180 / m_pi));
        /*
        std::cout<<"wp_x : "<<wp_x<<"\n""wp_y : "<<wp_y<<"\n";
        std::cout<<"x_to_lat (in degrees) : "<<x_to_lat<<"\n""y_to_long (in degrees)  : "<<y_to_long<<"\n";
        std::cout<<"parameter d : "<<some_parameter_d<<"\n"<<"\n""bearing : "<<bearing<<"\n\n";
        */
    }
} // end of namespace function
