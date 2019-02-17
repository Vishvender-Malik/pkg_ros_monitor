/* Author : Vishvender Malik
Email : vishvenderm@iiitd.ac.in
File : node_monitor_geo_fence.cpp
*/

#include "monitor_geo_fence.h"

//-------------------------------------------------------------------------------------------------------------------------------

//<-------------------------------------------Global variables and structures--------------------------------------------------->

std::string topic_guidance_velocity = "";
std::string topic_local_position_data = "";
std::string topic_corrected_velocity = ""; 
std::string topic_current_mavros_state = "";
std::string mode_flight = "";

const int array_velocity_guidance_size = 3, array_local_position_pose_data_size = 3, monitor_geo_fence_triggered_size = 3;

std::string array_monitor_geo_fence_triggered[monitor_geo_fence_triggered_size] = "No"; // for geo fence monitor

double array_velocity_guidance[array_velocity_guidance_size] = {0}, array_local_position_pose_data[array_local_position_pose_data_size] = {0},
max_possible_pose_in_positive_x = 0.0, max_possible_pose_in_negative_x = 0.0, max_possible_pose_in_positive_y = 0.0, 
max_possible_pose_in_negative_y = 0.0, max_possible_pose_in_positive_z = 0.0, max_possible_pose_in_negative_z = 0.0,
fence_limit_to_consider_in_x, fence_limit_to_consider_in_y, fence_limit_to_consider_in_z,
dist_bet_fence_and_vehicle_x = 0.0, dist_bet_fence_and_vehicle_y = 0.0, dist_bet_fence_and_vehicle_z = 0.0,
critical_radius_from_fence_limit, radius_of_circle_of_influence_s, dist_bet_fence_and_vehicle_overall = 0.0,
angle_bet_fence_and_vehicle = 0.0, gradient_x = 0.0, gradient_y = 0.0, constant_beta = 0.0, resulting_velocity_of_vehicle = 0.0,
resulting_angle_theta = 0.0, critical_radius_start_from_home = 0.0;

int sign_vehicle_pose_x, sign_vehicle_pose_y, sign_vehicle_pose_z;

geometry_msgs::TwistStamped command_geometry_twist; // final command_geometry_twist message to be published
nav_msgs::Odometry command_nav_pose; // final command_nav_pose message to be published
mavros_msgs::SetMode command_mavros_set_mode; // final command_mavros_set_mode to be published

// publishers, subscribers and services for monitor_geo_fence
ros::Subscriber sub_guidance_velocity;
ros::Publisher pub_corrected_velocity;
ros::Subscriber sub_local_position_data;
ros::Subscriber sub_current_mavros_state;
ros::ServiceClient srv_mavros_state;

//<----------------------------------------------------------------------------------------------------------------------------->

//<------------------------------------------Local function declarations--------------------------------------------------------->

void publish_final_command_geo_fence();
void receive_guidance_velocity(const geometry_msgs::TwistStamped::ConstPtr& data);
void set_topic_guidance_velocity(std::string guidance_velocity);
void set_max_possible_pose_in_positive_x(int config_max_possible_pose_in_positive_x);
void set_max_possible_pose_in_negative_x(int config_max_possible_pose_in_negative_x);
void set_max_possible_pose_in_positive_y(int config_max_possible_pose_in_positive_y);
void set_max_possible_pose_in_negative_y(int config_max_possible_pose_in_negative_y);
void set_max_possible_pose_in_positive_z(int config_max_possible_pose_in_positive_z);
void set_max_possible_pose_in_negative_z(int config_max_possible_pose_in_negative_z);
void set_topic_local_position_data(std::string local_position_data);
void set_topic_corrected_velocity(std::string corrected_velocity);
void set_constant_beta(double config_constant_beta);
void set_critical_radius_start_from_home(double config_critical_radius_start_from_home);
void set_critical_radius_from_fence_limit(double critical_radius);
void set_radius_of_circle_of_influence_s(double radius_of_circle_of_influence);
void receive_local_position_data(const nav_msgs::Odometry::ConstPtr &data);
void prediction_from_monitor_geo_fence();

//<----------------------------------------------------------------------------------------------------------------------------->

//<------------------------------------------------Initialize monitor node------------------------------------------------------>

int main(int argc, char **argv)
{
    ROS_INFO("\n\n---------------------------------Welcome--------------------------------------\n\n");

    // initialize ros node with a node name
    ros::init(argc, argv, "node_monitor_geo_fence");
    
    // create monitor object
    monitor_geo_fence obj_monitor_geo_fence;
    // implement class functions
    obj_monitor_geo_fence.init_parameter_server();
    //obj_monitor_geo_fence.set_monitor_topics(config, level); // main issue is here, not using same config as before
    obj_monitor_geo_fence.initialize_pub_and_sub();
    obj_monitor_geo_fence.monitor_start();
    
    return 0;

} // end of initialization

//<------------------------------------------Function definitions---------------------------------------------------------------->

monitor_geo_fence::monitor_geo_fence() : monitor_base(){
    ROS_INFO("monitor_geo_fence constructor called, object initialized\n");
}

void monitor_geo_fence::set_monitor_topics(pkg_ros_monitor::monitor_Config &config, uint32_t level){
    
    ROS_INFO("set_monitor_topics function reached\n");
    
    // for geo fence monitor
    set_topic_guidance_velocity(config.set_topic_guidance_velocity.c_str());
    set_max_possible_pose_in_positive_x(config.set_max_possible_pose_in_positive_x);
    set_max_possible_pose_in_negative_x(config.set_max_possible_pose_in_negative_x);
    set_max_possible_pose_in_positive_y(config.set_max_possible_pose_in_positive_y);
    set_max_possible_pose_in_negative_y(config.set_max_possible_pose_in_negative_y);
    set_max_possible_pose_in_positive_z(config.set_max_possible_pose_in_positive_z);
    set_max_possible_pose_in_negative_z(config.set_max_possible_pose_in_negative_z);
    set_critical_radius_start_from_home(config.set_critical_radius_start_from_home);
    set_critical_radius_from_fence_limit(config.set_critical_radius_from_fence_limit);
    set_radius_of_circle_of_influence_s(config.set_radius_of_circle_of_influence_s);
    set_constant_beta(config.set_constant_beta);
    set_topic_local_position_data(config.set_topic_local_position_data.c_str());
    set_topic_corrected_velocity(config.set_topic_corrected_velocity.c_str());

    ROS_INFO("Current geo fence configuration parameters: \n\n"
    "Guidance controller topic to get velocity parameters for comparison from : %s\n\n"
    "Fence limit set in positive x : %f\n"
    "Fence limit set in negative x : %f\n"
    "Fence limit set in positive y : %f\n"
    "Fence limit set in negative y : %f\n"
    "Fence limit set in positive z : %f\n"
    "Fence limit set in negative z : %f\n\n"
    "Critical radius (calculating from home) : %f\n"
    "Critical radius from fence limit : %f\n"
    "Radius of circle of influence \"s\" : %f\n\n"
    "Value for constant beta (for potential field calculation) : %f\n\n"
    "Topic to get local position data from : %s\n"
    "Topic to get desired velocity parameters from : %s\n"
    "Topic to publish corrected velocity to : %s \n\n",
    config.set_topic_guidance_velocity.c_str(),
    config.set_max_possible_pose_in_positive_x, config.set_max_possible_pose_in_negative_x,
    config.set_max_possible_pose_in_positive_y, config.set_max_possible_pose_in_negative_y,
    config.set_max_possible_pose_in_positive_z, config.set_max_possible_pose_in_negative_z,
    config.set_critical_radius_start_from_home,
    config.set_critical_radius_from_fence_limit, config.set_radius_of_circle_of_influence_s,
    config.set_constant_beta,
    config.set_topic_local_position_data.c_str(),
    config.set_topic_desired_airspeed.c_str(),
    config.set_topic_corrected_velocity.c_str());

    ROS_INFO("set_monitor_topics function ended\n");
}

void monitor_geo_fence::initialize_pub_and_sub(){
    
    ROS_INFO("start of initialize pub and sub function reached\n");
    // create a nodehandle to enable interaction with ros commands, usually always just after ros::init
    ros::NodeHandle nodeHandle;

    // final publisher to application // topic should just be cmd_vel
    pub_corrected_velocity = nodeHandle.advertise<geometry_msgs::TwistStamped>(topic_corrected_velocity, 1000);
    // publisher to publish new mavros flight state
    //pub_new_mavros_state = nodeHandle.advertise<mavros_msgs::State>(topic_new_mavros_state, 1000); 
    // subscriber to receive local position data from controller
    sub_local_position_data = nodeHandle.subscribe(topic_local_position_data, 1000, receive_local_position_data);
    // subscriber to receive velocity commands from the topic itself
    sub_guidance_velocity = nodeHandle.subscribe(topic_guidance_velocity, 1000, receive_guidance_velocity);
    // subscriber to receive current mavros state information
    //sub_current_mavros_state = nodeHandle.subscribe(topic_current_mavros_state, 1000, receive_current_mavros_state);
    // service to set mode of the vehicle
    srv_mavros_state = nodeHandle.serviceClient<mavros_msgs::SetMode>("SetMode");

    ROS_INFO("end of initialize pub and sub function reached\n");
}

void monitor_geo_fence::monitor_start(){

    ROS_INFO("start of monitor_start function reached");
    // get single wind monitor instance
    //monitor_wind::getInstance().initialize_pub_and_sub();
    //ROS_INFO("monitor_wind::getInstance function executed");
    //monitor_wind::initialize_pub_and_sub();

    //run loop at (10) Hz (always in decimal and faster than what is published through guidance controller)
    ros::Rate loop_rate(10);
    
    // Use current time as seed for random generator 
    srand(time(0)); 

    while (ros::ok())
    {
        // keep calling parameter server callback function to check for changes in parameters
        // kind of redundant to keep calling it since the parameters are set the first time it is called before
        // but here it is called to display ROS_INFO about the parameters, otherwise ROS_INFO is displayed only once in the 
        // beginning of the output
        //parameter_server.setCallback(callback_variable); 
        //monitor_wind::initialize_pub_and_sub();
        ros::spinOnce(); // if we have subscribers in our node, but always keep for good measure
        
        publish_final_command_geo_fence(); // keep calling this function
        
        // sleep for appropriate time to hit mark of (10) Hz
        loop_rate.sleep();
    } // end of while loop
    ROS_INFO("end of monitor_start function reached");
}

// function to set guidance velocity topic
void set_topic_guidance_velocity(std::string guidance_velocity)
{
    topic_guidance_velocity = guidance_velocity;
}

//functions to get pose data in positive and negative directions
void set_max_possible_pose_in_positive_x(int config_max_possible_pose_in_positive_x)
{
    max_possible_pose_in_positive_x = config_max_possible_pose_in_positive_x;
}

void set_max_possible_pose_in_negative_x(int config_max_possible_pose_in_negative_x)
{
    max_possible_pose_in_negative_x = config_max_possible_pose_in_negative_x;
}

void set_max_possible_pose_in_positive_y(int config_max_possible_pose_in_positive_y)
{
    max_possible_pose_in_positive_y = config_max_possible_pose_in_positive_y;
}

void set_max_possible_pose_in_negative_y(int config_max_possible_pose_in_negative_y)
{
    max_possible_pose_in_negative_y = config_max_possible_pose_in_negative_y;
}

void set_max_possible_pose_in_positive_z(int config_max_possible_pose_in_positive_z)
{
    max_possible_pose_in_positive_z = config_max_possible_pose_in_positive_z;
}

void set_max_possible_pose_in_negative_z(int config_max_possible_pose_in_negative_z)
{
    max_possible_pose_in_negative_z = config_max_possible_pose_in_negative_z;
}

void set_critical_radius_start_from_home(double config_critical_radius_start_from_home)
{
    critical_radius_start_from_home = config_critical_radius_start_from_home;
}

void set_critical_radius_from_fence_limit(double critical_radius)
{
    critical_radius_from_fence_limit = critical_radius;
}

void set_radius_of_circle_of_influence_s(double radius_of_circle_of_influence)
{
    radius_of_circle_of_influence_s = radius_of_circle_of_influence;
}

void set_constant_beta(double config_constant_beta)
{
    constant_beta = config_constant_beta;
}

// function to set local position data topic
void set_topic_local_position_data(std::string local_position_data)
{
    topic_local_position_data = local_position_data;
}

// function to set corrected position data topic
void set_topic_corrected_velocity(std::string corrected_velocity)
{
    topic_corrected_velocity = corrected_velocity;
}

// function to receive Desired airspeed (published on to the topic) from the topic itself
void receive_guidance_velocity(const geometry_msgs::TwistStamped::ConstPtr& data)
{
    array_velocity_guidance[0] = data -> twist.linear.x;
    array_velocity_guidance[1] = data -> twist.linear.y;
    array_velocity_guidance[2] = data -> twist.linear.z;
    ROS_INFO("Data received from topic \"/mavros/local_position/velocity\".");
}

// function to receive local position data
void receive_local_position_data(const nav_msgs::Odometry::ConstPtr &data)
{
    array_local_position_pose_data[0] = data -> pose.pose.position.x;
    array_local_position_pose_data[1] = data -> pose.pose.position.y;
    array_local_position_pose_data[2] = data -> pose.pose.position.z;
    ROS_INFO("Data received from topic \"mavros/global_position/local\".");
}

// function to receive current mavros state data
/*void receive_current_mavros_state(const mavros_msgs::State::ConstPtr &data)
{
    mode_guided = data -> guided;
    mode_flight = data -> mode;
}*/

// function to calculate required predictions and populate "command_nav_pose" message
void prediction_from_monitor_geo_fence()
{
    /* APPROACH 1
    If the vehicle sways more than the threshold (set in config file) values either side,
    trigger the monitor and publish velocity value 0 (hover at that position).
    */
   
    // get the sign for direction purposes
    //sign_local_position_y = copysign(1, array_local_position_pose_data[1]);

    // since we're not predicting anything in directions x and z,
    // pass the received velocity values as is
    //command_geometry_twist.twist.linear.x = array_velocity_guidance[0];
    //command_geometry_twist.twist.linear.z = array_velocity_guidance[1];

    // or if we were to :
    /*
    // predict which fence limit to consider for calculation (+ve or -ve)
   // get signs of vehicle position direction
   sign_vehicle_pose_x = copysign(1, array_local_position_pose_data[0]);
   sign_vehicle_pose_y = copysign(1, array_local_position_pose_data[1]);
   sign_vehicle_pose_z = copysign(1, array_local_position_pose_data[2]);

   if(sign_vehicle_pose_x == 1)
   {
       fence_limit_to_consider_in_x = max_possible_pose_in_positive_x;
   }
   else
   {
       fence_limit_to_consider_in_x = max_possible_pose_in_negative_x;
   }

   if(sign_vehicle_pose_y == 1)
   {
       fence_limit_to_consider_in_y = max_possible_pose_in_positive_y;
   }
   else
   {
       fence_limit_to_consider_in_y = max_possible_pose_in_negative_y;
   }

   if(sign_vehicle_pose_z == 1)
   {
       fence_limit_to_consider_in_z = max_possible_pose_in_positive_z;
   }
   else
   {
       fence_limit_to_consider_in_z = max_possible_pose_in_negative_z;
   }

    // in direction x :
    if(array_local_position_pose_data[0] <= critical_radius_start_from_home && 
    array_local_position_pose_data[0] >= (-1 * critical_radius_start_from_home))
    {
        array_monitor_geo_fence_triggered[0] = "No.";
        
        if(array_monitor_geo_fence_triggered[1] == "Yes." || array_monitor_geo_fence_triggered[2] == "Yes.")
        {
            // keep hovering at that position
            command_geometry_twist.twist.linear.x = 0;
            command_geometry_twist.twist.linear.y = 0; 
            command_geometry_twist.twist.linear.z = 0;
        }
        else
        {
            command_geometry_twist.twist.linear.x = array_velocity_guidance[0];
        } // end of inner if - else
    }
    else
    {
        array_monitor_geo_fence_triggered[0] = "Yes.";

        if(abs(array_local_position_pose_data[0]) < abs((abs(fence_limit_to_consider_in_x) + critical_radius_start_from_home) / 2))
        {
            command_geometry_twist.twist.linear.x = array_velocity_guidance[0];
        }
        else
        {
            // keep hovering at that position
            command_geometry_twist.twist.linear.x = 0;
            command_geometry_twist.twist.linear.y = 0; 
            command_geometry_twist.twist.linear.z = 0;
        } // end of inner if - else
    } // end of outer if - else

    // in direction y :
    if(array_local_position_pose_data[1] <= critical_radius_start_from_home && 
    array_local_position_pose_data[1] >= (-1 * critical_radius_start_from_home))
    {
        array_monitor_geo_fence_triggered[1] = "No.";
        
        if(array_monitor_geo_fence_triggered[0] == "Yes." || array_monitor_geo_fence_triggered[2] == "Yes.")
        {
            // keep hovering at that position
            command_geometry_twist.twist.linear.x = 0;
            command_geometry_twist.twist.linear.y = 0; 
            command_geometry_twist.twist.linear.z = 0;
        }
        else
        {
            command_geometry_twist.twist.linear.y = array_velocity_guidance[1];
        } // end of inner if - else
    }
    else
    {
        array_monitor_geo_fence_triggered[1] = "Yes.";

        if(abs(array_local_position_pose_data[1]) < abs((abs(fence_limit_to_consider_in_y) + critical_radius_start_from_home) / 2))
        {
            command_geometry_twist.twist.linear.x = array_velocity_guidance[1];
        }
        else
        {
            // keep hovering at that position
            command_geometry_twist.twist.linear.x = 0;
            command_geometry_twist.twist.linear.y = 0; 
            command_geometry_twist.twist.linear.z = 0;
        } // end of inner if - else
    } // end of outer if - else

    // in direction z :
    if(array_local_position_pose_data[2] <= critical_radius_start_from_home && 
    array_local_position_pose_data[2] >= (-1 * critical_radius_start_from_home))
    {
        array_monitor_geo_fence_triggered[2] = "No.";
        
        if(array_monitor_geo_fence_triggered[0] == "Yes." || array_monitor_geo_fence_triggered[1] == "Yes.")
        {
            // keep hovering at that position
            command_geometry_twist.twist.linear.x = 0;
            command_geometry_twist.twist.linear.y = 0; 
            command_geometry_twist.twist.linear.z = 0;
        }
        else
        {
            command_geometry_twist.twist.linear.z = array_velocity_guidance[2];
        } // end of inner if - else
    }
    else
    {
        array_monitor_geo_fence_triggered[2] = "Yes.";

        if(abs(array_local_position_pose_data[2]) < abs((abs(fence_limit_to_consider_in_z) + critical_radius_start_from_home) / 2))
        {
            command_geometry_twist.twist.linear.x = array_velocity_guidance[2];
        }
        else
        {
            // keep hovering at that position
            command_geometry_twist.twist.linear.x = 0;
            command_geometry_twist.twist.linear.y = 0; 
            command_geometry_twist.twist.linear.z = 0;
        } // end of inner if - else
    } // end of outer if - else   
    */
    /*
    APPROACH 2
    Vehicle is by default in "AUTO" mode when in motion.
    Geo fence consists of the boundary point as well as a buffer radius.
    As soon as the vehicle breaches the buffer radius, change the mode to 
    "GUIDED" and bring it within safe zone. When it comes back in safe zone change the
    mode back to "AUTO" so that mission can be resumed from that position.
    Use Potential Fields concept to calculate new velocities.
    */
   
   // predict which fence limit to consider for calculation (+ve or -ve)
   // get signs of vehicle position direction
   sign_vehicle_pose_x = copysign(1, array_local_position_pose_data[0]);
   sign_vehicle_pose_y = copysign(1, array_local_position_pose_data[1]);
   sign_vehicle_pose_z = copysign(1, array_local_position_pose_data[2]);

   if(sign_vehicle_pose_x == 1)
   {
       fence_limit_to_consider_in_x = max_possible_pose_in_positive_x;
   }
   else
   {
       fence_limit_to_consider_in_x = max_possible_pose_in_negative_x;
   }

   if(sign_vehicle_pose_y == 1)
   {
       fence_limit_to_consider_in_y = max_possible_pose_in_positive_y;
   }
   else
   {
       fence_limit_to_consider_in_y = max_possible_pose_in_negative_y;
   }

   if(sign_vehicle_pose_z == 1)
   {
       fence_limit_to_consider_in_z = max_possible_pose_in_positive_z;
   }
   else
   {
       fence_limit_to_consider_in_z = max_possible_pose_in_negative_z;
   }

   dist_bet_fence_and_vehicle_x = fence_limit_to_consider_in_x - array_local_position_pose_data[0];
   dist_bet_fence_and_vehicle_y = fence_limit_to_consider_in_y - array_local_position_pose_data[1];
   dist_bet_fence_and_vehicle_z = fence_limit_to_consider_in_z - array_local_position_pose_data[2]; // not used so far

   // calculations for potential field based velocities in two dimensions
    //dist_bet_fence_and_vehicle_overall = sqrt((pow(dist_bet_fence_and_vehicle_x, 2.0)) + (pow(dist_bet_fence_and_vehicle_y, 2.0))); //wrong
    angle_bet_fence_and_vehicle = atan2((dist_bet_fence_and_vehicle_y), dist_bet_fence_and_vehicle_x); // in radians

    // actions to be taken
    // in direction x : 
    if(dist_bet_fence_and_vehicle_x < critical_radius_from_fence_limit)
    {
        //array_monitor_geo_fence_triggered[2] = "Yes.";
        command_mavros_set_mode.request.base_mode = 216; // mode : GUIDED ARMED
        command_mavros_set_mode.request.custom_mode = "GUIDED";

        // keep hovering at that position, delta x and delta y in potential field equations
        command_geometry_twist.twist.linear.x = 0;
        command_geometry_twist.twist.linear.y = 0; 
        command_geometry_twist.twist.linear.z = 0;
    }
    else if ((dist_bet_fence_and_vehicle_x > critical_radius_from_fence_limit) && 
    (dist_bet_fence_and_vehicle_x < (critical_radius_from_fence_limit + radius_of_circle_of_influence_s))) 
    {
        command_mavros_set_mode.request.base_mode = 216; // mode : GUIDED ARMED
        command_mavros_set_mode.request.custom_mode = "GUIDED";        
        
        gradient_x = - constant_beta;
        gradient_y = 0;

        resulting_velocity_of_vehicle = sqrt(pow(gradient_x, 2.0) + pow(gradient_y, 2.0));
        command_geometry_twist.twist.linear.x = resulting_velocity_of_vehicle * cos(angle_bet_fence_and_vehicle);
    }        
    else if(dist_bet_fence_and_vehicle_x > (critical_radius_from_fence_limit + radius_of_circle_of_influence_s))
    {
        command_mavros_set_mode.request.base_mode = 220; // mode : AUTO ARMED
        command_mavros_set_mode.request.custom_mode = "AUTO";

        command_geometry_twist.twist.linear.x = array_velocity_guidance[0];
    }  

    // in direction y :
    if(dist_bet_fence_and_vehicle_y < critical_radius_from_fence_limit)
    {
        //array_monitor_geo_fence_triggered[2] = "Yes.";
        command_mavros_set_mode.request.base_mode = 216; // mode : GUIDED ARMED
        command_mavros_set_mode.request.custom_mode = "GUIDED";

        // keep hovering at that position, delta x and delta y in potential field equations
        command_geometry_twist.twist.linear.x = 0;
        command_geometry_twist.twist.linear.y = 0; 
        command_geometry_twist.twist.linear.z = 0;
    }
    else if ((dist_bet_fence_and_vehicle_y > critical_radius_from_fence_limit) && 
    (dist_bet_fence_and_vehicle_y < (critical_radius_from_fence_limit + radius_of_circle_of_influence_s))) 
    {
        command_mavros_set_mode.request.base_mode = 216; // mode : GUIDED ARMED
        command_mavros_set_mode.request.custom_mode = "GUIDED";        
        
        gradient_x = 0;
        gradient_y = - constant_beta;

        resulting_velocity_of_vehicle = sqrt(pow(gradient_x, 2.0) + pow(gradient_y, 2.0));
        command_geometry_twist.twist.linear.y = resulting_velocity_of_vehicle * sin(angle_bet_fence_and_vehicle);
    }        
    else if(dist_bet_fence_and_vehicle_y > (critical_radius_from_fence_limit + radius_of_circle_of_influence_s))
    {
        command_mavros_set_mode.request.base_mode = 220; // mode : AUTO ARMED
        command_mavros_set_mode.request.custom_mode = "AUTO";

        command_geometry_twist.twist.linear.y = array_velocity_guidance[1];
    }  
    
    // for direction z
    command_geometry_twist.twist.linear.z = array_velocity_guidance[2];
    
    /*
    if(dist_bet_fence_and_vehicle_overall < critical_radius_from_fence_limit)
    {
        //array_monitor_geo_fence_triggered[2] = "Yes.";
        command_mavros_set_mode.request.base_mode = 216; // mode : GUIDED ARMED
        command_mavros_set_mode.request.custom_mode = "GUIDED";

        // keep hovering at that position, delta x and delta y in potential field equations
        command_geometry_twist.twist.linear.x = 0;
        command_geometry_twist.twist.linear.y = 0; 
        command_geometry_twist.twist.linear.z = 0;
    }
    else if ((dist_bet_fence_and_vehicle_overall > critical_radius_from_fence_limit) && 
    (dist_bet_fence_and_vehicle_overall < (critical_radius_from_fence_limit + radius_of_circle_of_influence_s))) 
    {
        command_mavros_set_mode.request.base_mode = 216; // mode : GUIDED ARMED
        command_mavros_set_mode.request.custom_mode = "GUIDED";
        /*
        gradient_x = - constant_beta * (radius_of_circle_of_influence_s + critical_radius_from_fence_limit - dist_bet_fence_and_vehicle_overall)
                        * cos(angle_bet_fence_and_vehicle);
        gradient_y = - constant_beta * (radius_of_circle_of_influence_s + critical_radius_from_fence_limit - dist_bet_fence_and_vehicle_overall)
                        * sin(angle_bet_fence_and_vehicle);;

        resulting_velocity_of_vehicle = sqrt(pow(gradient_x, 2.0) + pow(gradient_y, 2.0));
        resulting_angle_theta = atan2(gradient_y, gradient_x);
        command_geometry_twist.twist.linear.x = resulting_velocity_of_vehicle * cos(resulting_angle_theta);
        command_geometry_twist.twist.linear.y = resulting_velocity_of_vehicle * sin(resulting_angle_theta);
        
        
        // for direction x
        if((dist_bet_fence_and_vehicle_x > critical_radius_from_fence_limit) && 
        (dist_bet_fence_and_vehicle_x < (critical_radius_from_fence_limit + radius_of_circle_of_influence_s)))
        {
            //gradient_x = - constant_beta * (radius_of_circle_of_influence_s + critical_radius_from_fence_limit - dist_bet_fence_and_vehicle_overall)
            //            * cos(angle_bet_fence_and_vehicle);
            gradient_x = - constant_beta;
            gradient_y = 0;

            resulting_velocity_of_vehicle = sqrt(pow(gradient_x, 2.0) + pow(gradient_y, 2.0));
            command_geometry_twist.twist.linear.x = resulting_velocity_of_vehicle * cos(angle_bet_fence_and_vehicle);
        }  
        // for direction y
        if((dist_bet_fence_and_vehicle_y > critical_radius_from_fence_limit) && 
        (dist_bet_fence_and_vehicle_y < (critical_radius_from_fence_limit + radius_of_circle_of_influence_s)))
        {
            gradient_x = 0;
            gradient_y = - constant_beta;
            //gradient_y = - constant_beta * (radius_of_circle_of_influence_s + critical_radius_from_fence_limit - dist_bet_fence_and_vehicle_overall)
            //            * cos(angle_bet_fence_and_vehicle);
 
            resulting_velocity_of_vehicle = sqrt(pow(gradient_x, 2.0) + pow(gradient_y, 2.0));
            command_geometry_twist.twist.linear.y = resulting_velocity_of_vehicle * sin(angle_bet_fence_and_vehicle);
        }  
        
        // for direction z
        command_geometry_twist.twist.linear.z = array_velocity_guidance[2];
        /*
        resulting_velocity_of_vehicle = sqrt(pow(gradient_x, 2.0) + pow(gradient_y, 2.0));

        // split resulting velocity into parameters x and y
        command_geometry_twist.twist.linear.x = resulting_velocity_of_vehicle * cos(angle_bet_fence_and_vehicle);
        command_geometry_twist.twist.linear.y = resulting_velocity_of_vehicle * sin(angle_bet_fence_and_vehicle);
        
    }
    else if(dist_bet_fence_and_vehicle_overall > (critical_radius_from_fence_limit + radius_of_circle_of_influence_s))
    {
        command_mavros_set_mode.request.base_mode = 220; // mode : AUTO ARMED
        command_mavros_set_mode.request.custom_mode = "AUTO";

        command_geometry_twist.twist.linear.x = array_velocity_guidance[0];
        command_geometry_twist.twist.linear.y = array_velocity_guidance[1];
        command_geometry_twist.twist.linear.z = array_velocity_guidance[2];
    }  
     */ 
} // end of function prediction_from_monitor_geo_fence()

// function to publish final command_geometry_twist through the publisher via this monitor
void publish_final_command_geo_fence()
{
    ROS_INFO("\n\n------------------------------------Data received----------------------------------------\n\n");

    // make prediction at set frequency
    prediction_from_monitor_geo_fence();
    
    ROS_INFO("\n\nGeo fence limits set :\n\n"
    "Fence limit set in direction positive x : %f \n"
    "Fence limit set in direction negative x : %f \n"
    "Fence limit set in direction positive y : %f \n"
    "Fence limit set in direction negative y : %f \n"
    "Fence limit set in direction positive z : %f \n"
    "Fence limit set in direction negative z : %f \n\n"
    "Critical radius (calculating from home location) : %f\n"
    "Critical radius from fence limit : %f\n"
    "Radius of circle of influence \"s\" : %f\n",
    max_possible_pose_in_positive_x, max_possible_pose_in_negative_x,
    max_possible_pose_in_positive_y, max_possible_pose_in_negative_y,
    max_possible_pose_in_positive_z, max_possible_pose_in_negative_z,
    critical_radius_start_from_home, critical_radius_from_fence_limit, radius_of_circle_of_influence_s);
    
    ROS_INFO("\n\nLocal position received from 'Home' in direction x : %f \n"
    "Local position received from \"Home\" in direction y : %f \n"
    "Local position received from \"Home\" in direction z : %f \n",
    array_local_position_pose_data[0], array_local_position_pose_data[1], array_local_position_pose_data[2]);
    /*
    ROS_INFO("\n\nDistance between fence limit and vehicle in direction x : %f\n"
    "Distance between fence limit and vehicle in direction y : %f\n"
    "Distance between fence limit and vehicle in direction z : %f\n",
    dist_bet_fence_and_vehicle_x, dist_bet_fence_and_vehicle_y, dist_bet_fence_and_vehicle_z);

    ROS_INFO("\n\nDistance between fence and vehicle overall : %f\n"
    "Current angle between fence and vehicle (in radians) : %f\n"
    "Gradient x : %f\n"
    "Gradient y : %f\n"
    "Resulting velocity of vehicle : %f\n"
    "Resulting angle theta : %f\n",
    dist_bet_fence_and_vehicle_overall, angle_bet_fence_and_vehicle,
    gradient_x, gradient_y, resulting_velocity_of_vehicle,
    resulting_angle_theta);
    */
    ROS_INFO("\n\nDesired airspeed received via controller in direction x : %f \n""Desired airspeed received via controller in direction y : %f \n"
    "Desired airspeed received via controller in direction z : %f \n", array_velocity_guidance[0], array_velocity_guidance[1],
    array_velocity_guidance[2]);
    /*
    ROS_INFO("\n\nCurrent mode set request : %s\n"
    "Requested mode actually set : %d\n"
    "Value of constant beta : %f\n",
    command_mavros_set_mode.request.custom_mode.c_str(),
    command_mavros_set_mode.response.mode_sent,
    constant_beta);
    */
    ROS_INFO("\n\nFence breach in direction x : %s \n"
    "Fence breach in direction y : %s \n"
    "Fence breach in direction z : %s \n",
    array_monitor_geo_fence_triggered[0].c_str(),
    array_monitor_geo_fence_triggered[1].c_str(),
    array_monitor_geo_fence_triggered[2].c_str());
    
    ROS_INFO("\n\nCorrected airspeed published by monitor in direction x : %f \n"
    "Corrected airspeed published by monitor in direction y : %f \n"
    "Corrected airspeed published by monitor in direction z : %f \n",
    command_geometry_twist.twist.linear.x, command_geometry_twist.twist.linear.y, command_geometry_twist.twist.linear.z);
    ROS_INFO("\n\n\n---------------------------------------------------------------------------------------------\n");
    
    // finally, call the service and publish to the topic
    srv_mavros_state.call(command_mavros_set_mode);
    pub_corrected_velocity.publish(command_geometry_twist);
    ROS_INFO("Data publishing to topic \"/mavros/setpoint_velocity/cmd_vel\".");
    ROS_INFO("\n\n------------------------------------End of data block----------------------------------------\n\n");
} // end of function publish_final_command_geo_fence()