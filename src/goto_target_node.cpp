#include <cstdio>  // Standard Input and Output Library
#include "rclcpp/rclcpp.hpp"
#include "turtlesim/msg/pose.hpp"  // Include turtlesim for the turtle's pose
#include "geometry_msgs/msg/pose.hpp"  // Include geometry_msgs for the target pose
#include "geometry_msgs/msg/twist.hpp"
#include "endicott_interfaces/msg/move_status.hpp" 
#include "../include/endicott_util.hpp"

// Include the custom MoveStatus.msg message
#include <cmath>
#include <memory>
#include <stdexcept>
#include <fmt/core.h>

#define HOME_X 5.444445
#define HOME_Y 5.444445
#define HOME_THETA 0.0

enum GotoMoveStates{ INIT, NEW_TARGET, POINT_AT_TARGET, MOVE_TO_TARGET, ORIENTATING, AT_TARGET_POSE};

class GotoNode : public rclcpp::Node {
public:
  GotoNode() : Node("goto_node") {

    // Subscription to command target Pose (geometry_msgs::msg::Pose)
      target_pose_subscription_ = create_subscription<geometry_msgs::msg::Pose>(
        "/target_pose", 10, std::bind(&GotoNode::target_pose_callback, this, std::placeholders::_1));
  
    // Subscription to the turtle's pose (turtlesim::msg::Pose)
      pose_subscription_  = create_subscription<turtlesim::msg::Pose>(
        "/turtle1/pose", 10, std::bind(&GotoNode::pose_callback, this, std::placeholders::_1));

    // Publisher a comand vel msg to turtlesim
    velocity_publisher_ = create_publisher<geometry_msgs::msg::Twist>("/turtle1/cmd_vel", 10);

    // Publisher a move status message
    goto_status_publisher_ = create_publisher<endicott_interfaces::msg::MoveStatus>("/goto_status", 10);

    RCLCPP_INFO(this->get_logger(), " ----- start target set: x=%.2f, y=%.2f, theta=%.2f  -----", target_x, target_y, target_theta);
    }

private:

  void target_pose_callback(const geometry_msgs::msg::Pose::SharedPtr msg) {
    // Update target position based on the received pose message
    target_x     = msg->position.x;
    target_y     = msg->position.y;
    target_theta = msg->orientation.z;
    goto_move_state = NEW_TARGET;

    distance_to_target = cal_relative_polar_distance(target_x, target_y, current_x, current_y);
    angle_to_target    = cal_relative_polar_angle(target_x, target_y, current_x, current_y);
    //double angular_error = unwrap(angle_to_target - current_theta);

    // let the sequancer know we got the new target
    publish_goto_state("NEW_TARGET", distance_to_target, angle_to_target );
    RCLCPP_INFO(this->get_logger(), " ----- NEW_TARGET: target_x=%.2f, target_y=%.2f, target_theta=%.2f  -----", target_x, target_y, target_theta);
    }

  void pose_callback(const turtlesim::msg::Pose::SharedPtr msg) {
    current_x     = msg->x;
    current_y     = msg->y;
    current_theta = msg->theta;

    //comment out this line
    move_to_target(); 

    //uncomment out this line to run your code
    //void move_to_target_with_constraints();  
    }


  // --- Your new functions that use constraint V_MAX an A_MAX ------------------------

  /* -- delete this line
  // constraints for velocity and acceleration 
  #define V_MAX 4
  #define A_MAX 2

  void move_to_target_with_constraints(){
  }

  void rotate_turtle_with_constraints(double angular_error) {
  }

  void move_forward_and_rotate_with_constraints(double distance, double angular_error) {
  }

  float cal_next_vel_with_constraints(double current_dPos, double current_vel,  ) {
  }

  */ // -- delete this line



  void move_to_target() {
    distance_to_target = cal_relative_polar_distance(target_x, target_y, current_x, current_y);
    angle_to_target    = cal_relative_polar_angle(target_x, target_y, current_x, current_y);
    double angular_error = unwrap(angle_to_target - current_theta);

    if(distance_to_target > 0.1){
      if(abs(angular_error) >0.1){
        rotate_turtle(angular_error);
        publish_goto_state("POINT_AT_TARGET", distance_to_target, angle_to_target );
        update_goto_move_state(POINT_AT_TARGET);
        }
      else{
        move_forward_and_rotate(distance_to_target, angular_error);
        publish_goto_state("MOVE_TO_TARGET", distance_to_target, angle_to_target );
        update_goto_move_state(MOVE_TO_TARGET);
        }
      }
    else{
      double final_theta_err = unwrap(target_theta - current_theta);
      if (abs(final_theta_err) > 0.1){
        rotate_turtle(final_theta_err);
        publish_goto_state("ORIENTATING", distance_to_target, angle_to_target );
        update_goto_move_state(ORIENTATING);
        }
      else{
        stop_turtle();
        publish_goto_state("AT_TARGET_POSE", distance_to_target, angle_to_target );
        update_goto_move_state(AT_TARGET_POSE);
        }
      }
    }

  void update_goto_move_state(GotoMoveStates new_goto_move_state){
    std::string state_string_name;

    if(goto_move_state != new_goto_move_state){
      
      goto_move_state = new_goto_move_state;
      if(goto_move_state == POINT_AT_TARGET)        { RCLCPP_INFO(this->get_logger(), "----- POINT_AT_TARGET -----");}
      else if (goto_move_state == MOVE_TO_TARGET)   { RCLCPP_INFO(this->get_logger(), "----- MOVE_TO_TARGET  -----");}
      else if (goto_move_state == ORIENTATING)      { RCLCPP_INFO(this->get_logger(), "----- ORIENTATING     -----");}
      else if (goto_move_state == AT_TARGET_POSE)   { RCLCPP_INFO(this->get_logger(), "----- AT_TARGET_POSE  -----");}
      else                                          { RCLCPP_INFO(this->get_logger(), "----- ??????          -----");}

      RCLCPP_INFO(this->get_logger(), "current_x=%.2f, current_y=%.2f,  current_theta=%.2f,", current_x, current_y, current_theta);
      }

    }


  void rotate_turtle(double angular_error) {
    geometry_msgs::msg::Twist twist_msg;
    twist_msg.angular.z = angular_error;
    velocity_publisher_->publish(twist_msg);
    }

  void move_forward_and_rotate(double distance, double angular_error) {
    geometry_msgs::msg::Twist twist_msg;
    twist_msg.linear.x = distance;
    twist_msg.angular.z = angular_error;
    velocity_publisher_->publish(twist_msg);
    }

  void stop_turtle() {
    geometry_msgs::msg::Twist twist_msg;
    velocity_publisher_->publish(twist_msg);
    }

  void publish_goto_state(const std::string& state, double distance_err, double angle_err ) {
    endicott_interfaces::msg::MoveStatus goto_status_msg;
    goto_status_msg.state          = state;
    goto_status_msg.distance_error = distance_err;
    goto_status_msg.theta_error    = angle_err;

    // Publish the message
    goto_status_publisher_->publish(goto_status_msg);
    }
  // Publisher and subscription variables
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr velocity_publisher_;
  rclcpp::Publisher<endicott_interfaces::msg::MoveStatus>::SharedPtr goto_status_publisher_;  // GotoState publisher
  rclcpp::Subscription<geometry_msgs::msg::Pose>::SharedPtr target_pose_subscription_;
  rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr pose_subscription_;

   // Variables 
  double target_x     = (float)HOME_X;
  double target_y     = (float)HOME_Y;
  double target_theta = (float)HOME_THETA;
  double current_x;
  double current_y;
  double current_theta;
  double distance_to_target;
  double angle_to_target;
  GotoMoveStates    goto_move_state = INIT;
  };

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto goto_node = std::make_shared<GotoNode>();

  rclcpp::spin(goto_node);

  rclcpp::shutdown();
  return 0; 
  }