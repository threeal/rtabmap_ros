/*
Copyright (c) 2010-2016, Mathieu Labbe - IntRoLab - Universite de Sherbrooke
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Universite de Sherbrooke nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef INCLUDE_RTABMAP_ROS_COMMONDATASUBSCRIBER_H_
#define INCLUDE_RTABMAP_ROS_COMMONDATASUBSCRIBER_H_

#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/sync_policies/exact_time.h>

#include <image_transport/image_transport.h>
#include <image_transport/subscriber_filter.h>

#include <cv_bridge/cv_bridge.h>

#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include <nav_msgs/msg/odometry.hpp>

#include <rtabmap_ros/msg/rgbd_image.hpp>
#include <rtabmap_ros/msg/user_data.hpp>
#include <rtabmap_ros/msg/odom_info.hpp>
#include <rtabmap_ros/CommonDataSubscriberDefines.h>

namespace rtabmap_ros {

class CommonDataSubscriber {
public:
	CommonDataSubscriber(rclcpp::Node& node, bool gui);
	virtual ~CommonDataSubscriber();

	bool isSubscribedToDepth() const  {return subscribedToDepth_;}
	bool isSubscribedToStereo() const {return subscribedToStereo_;}
	bool isSubscribedToRGB() const  {return subscribedToRGB_;}
	bool isSubscribedToOdom() const  {return subscribedToOdom_;}
	bool isSubscribedToRGBD() const   {return subscribedToRGBD_;}
	bool isSubscribedToScan2d() const {return subscribedToScan2d_;}
	bool isSubscribedToScan3d() const {return subscribedToScan3d_;}
	bool isSubscribedToOdomInfo() const {return subscribedToOdomInfo_;}
	bool isDataSubscribed() const {return isSubscribedToDepth() || isSubscribedToStereo() || isSubscribedToRGBD() || isSubscribedToScan2d() || isSubscribedToScan3d() || isSubscribedToRGB() || isSubscribedToOdom();}
	int rgbdCameras() const {return isSubscribedToRGBD()?(int)rgbdSubs_.size():0;}
	int getQueueSize() const {return queueSize_;}
	bool isApproxSync() const {return approxSync_;}

protected:
	void setupCallbacks(rclcpp::Node & node);
	virtual void commonDepthCallback(
				const nav_msgs::msg::Odometry::ConstSharedPtr & odomMsg,
				const rtabmap_ros::msg::UserData::ConstSharedPtr & userDataMsg,
				const std::vector<cv_bridge::CvImageConstPtr> & imageMsgs,
				const std::vector<cv_bridge::CvImageConstPtr> & depthMsgs,
				const std::vector<sensor_msgs::msg::CameraInfo> & cameraInfoMsgs,
				const sensor_msgs::msg::LaserScan::ConstSharedPtr& scanMsg,
				const sensor_msgs::msg::PointCloud2::ConstSharedPtr& scan3dMsg,
				const rtabmap_ros::msg::OdomInfo::ConstSharedPtr& odomInfoMsg) = 0;
	virtual void commonStereoCallback(
				const nav_msgs::msg::Odometry::ConstSharedPtr & odomMsg,
				const rtabmap_ros::msg::UserData::ConstSharedPtr & userDataMsg,
				const cv_bridge::CvImageConstPtr& leftImageMsg,
				const cv_bridge::CvImageConstPtr& rightImageMsg,
				const sensor_msgs::msg::CameraInfo& leftCamInfoMsg,
				const sensor_msgs::msg::CameraInfo& rightCamInfoMsg,
				const sensor_msgs::msg::LaserScan::ConstSharedPtr& scanMsg,
				const sensor_msgs::msg::PointCloud2::ConstSharedPtr& scan3dMsg,
				const rtabmap_ros::msg::OdomInfo::ConstSharedPtr& odomInfoMsg) = 0;
	virtual void commonLaserScanCallback(
				const nav_msgs::msg::Odometry::ConstSharedPtr & odomMsg,
				const rtabmap_ros::msg::UserData::ConstSharedPtr & userDataMsg,
				const sensor_msgs::msg::LaserScan::ConstSharedPtr& scanMsg,
				const sensor_msgs::msg::PointCloud2::ConstSharedPtr& scan3dMsg,
				const rtabmap_ros::msg::OdomInfo::ConstSharedPtr& odomInfoMsg) = 0;
	virtual void commonOdomCallback(
				const nav_msgs::msg::Odometry::ConstSharedPtr & odomMsg,
				const rtabmap_ros::msg::UserData::ConstSharedPtr & userDataMsg,
				const rtabmap_ros::msg::OdomInfo::ConstSharedPtr& odomInfoMsg) = 0;

	void commonSingleDepthCallback(
				const nav_msgs::msg::Odometry::ConstSharedPtr & odomMsg,
				const rtabmap_ros::msg::UserData::ConstSharedPtr & userDataMsg,
				const cv_bridge::CvImageConstPtr & imageMsg,
				const cv_bridge::CvImageConstPtr & depthMsg,
				const sensor_msgs::msg::CameraInfo & rgbCameraInfoMsg,
				const sensor_msgs::msg::CameraInfo & depthCameraInfoMsg,
				const sensor_msgs::msg::LaserScan::ConstSharedPtr& scanMsg,
				const sensor_msgs::msg::PointCloud2::ConstSharedPtr& scan3dMsg,
				const rtabmap_ros::msg::OdomInfo::ConstSharedPtr& odomInfoMsg);

private:
	void callbackCalled() {callbackCalled_ = true;}
	void setupDepthCallbacks(
			rclcpp::Node & node,
			bool subscribeOdom,
			bool subscribeUserData,
			bool subscribeScan2d,
			bool subscribeScan3d,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);
	void setupStereoCallbacks(
			rclcpp::Node & node,
			bool subscribeOdom,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);
	void setupRGBCallbacks(
			rclcpp::Node & node,
			bool subscribeOdom,
			bool subscribeUserData,
			bool subscribeScan2d,
			bool subscribeScan3d,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);
	void setupRGBDCallbacks(
			rclcpp::Node & node,
			bool subscribeOdom,
			bool subscribeUserData,
			bool subscribeScan2d,
			bool subscribeScan3d,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);
	void setupRGBD2Callbacks(
			rclcpp::Node & node,
			bool subscribeOdom,
			bool subscribeUserData,
			bool subscribeScan2d,
			bool subscribeScan3d,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);
	void setupRGBD3Callbacks(
			rclcpp::Node & node,
			bool subscribeOdom,
			bool subscribeUserData,
			bool subscribeScan2d,
			bool subscribeScan3d,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);
	void setupRGBD4Callbacks(
			rclcpp::Node & node,
			bool subscribeOdom,
			bool subscribeUserData,
			bool subscribeScan2d,
			bool subscribeScan3d,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);
	void setupScanCallbacks(
			rclcpp::Node & node,
			bool scan2dTopic,
			bool subscribeOdom,
			bool subscribeUserData,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);
	void setupOdomCallbacks(
			rclcpp::Node & node,
			bool subscribeUserData,
			bool subscribeOdomInfo,
			int queueSize,
			bool approxSync);

protected:
	std::string subscribedTopicsMsg_;
	int queueSize_;

private:
	bool approxSync_;
	std::thread* warningThread_;
	bool callbackCalled_;
	bool subscribedToDepth_;
	bool subscribedToStereo_;
	bool subscribedToRGB_;
	bool subscribedToOdom_;
	bool subscribedToRGBD_;
	bool subscribedToScan2d_;
	bool subscribedToScan3d_;
	bool subscribedToOdomInfo_;
	bool subscribedToUserData_;
	int rgbdCameras_;
	std::string name_;

	//for depth and rgb-only callbacks
	image_transport::SubscriberFilter imageSub_;
	image_transport::SubscriberFilter imageDepthSub_;
	message_filters::Subscriber<sensor_msgs::msg::CameraInfo> cameraInfoSub_;

	//for rgbd callback
	rclcpp::Subscription<rtabmap_ros::msg::RGBDImage>::ConstSharedPtr rgbdSub_;
	std::vector<message_filters::Subscriber<rtabmap_ros::msg::RGBDImage>*> rgbdSubs_;

	//stereo callback
	image_transport::SubscriberFilter imageRectLeft_;
	image_transport::SubscriberFilter imageRectRight_;
	message_filters::Subscriber<sensor_msgs::msg::CameraInfo> cameraInfoLeft_;
	message_filters::Subscriber<sensor_msgs::msg::CameraInfo> cameraInfoRight_;

	message_filters::Subscriber<nav_msgs::msg::Odometry> odomSub_;
	message_filters::Subscriber<rtabmap_ros::msg::UserData> userDataSub_;
	message_filters::Subscriber<sensor_msgs::msg::LaserScan> scanSub_;
	message_filters::Subscriber<sensor_msgs::msg::PointCloud2> scan3dSub_;
	message_filters::Subscriber<rtabmap_ros::msg::OdomInfo> odomInfoSub_;

	rclcpp::Subscription<sensor_msgs::msg::LaserScan>::ConstSharedPtr scan2dSubOnly_;
	rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::ConstSharedPtr scan3dSubOnly_;
	rclcpp::Subscription<nav_msgs::msg::Odometry>::ConstSharedPtr odomSubOnly_;

	// RGB + Depth
	DATA_SYNCS3(depth, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS4(depthScan2d, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan)
	DATA_SYNCS4(depthScan3d, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS4(depthInfo, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(depthScan2dInfo, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(depthScan3dInfo, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// RGB + Depth + Odom
	DATA_SYNCS4(depthOdom, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS5(depthOdomScan2d, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan)
	DATA_SYNCS5(depthOdomScan3d, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS5(depthOdomInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(depthOdomScan2dInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(depthOdomScan3dInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// RGB + Depth + User Data
	DATA_SYNCS4(depthData, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS5(depthDataScan2d, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan)
	DATA_SYNCS5(depthDataScan3d, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS5(depthDataInfo, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(depthDataScan2dInfo, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(depthDataScan3dInfo, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// RGB + Depth + Odom + User Data
	DATA_SYNCS5(depthOdomData, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS6(depthOdomDataScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan)
	DATA_SYNCS6(depthOdomDataScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS6(depthOdomDataInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS7(depthOdomDataScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS7(depthOdomDataScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// Stereo
	DATA_SYNCS4(stereo, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS5(stereoInfo, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)

	// Stereo + Odom
	DATA_SYNCS5(stereoOdom, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS6(stereoOdomInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)

	// RGB-only
	DATA_SYNCS2(rgb, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS3(rgbScan2d, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan)
	DATA_SYNCS3(rgbScan3d, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS3(rgbInfo, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(rgbScan2dInfo, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(rgbScan3dInfo, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// RGB-only + Odom
	DATA_SYNCS3(rgbOdom, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS4(rgbOdomScan2d, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan)
	DATA_SYNCS4(rgbOdomScan3d, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS4(rgbOdomInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbOdomScan2dInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbOdomScan3dInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// RGB-only + User Data
	DATA_SYNCS3(rgbData, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS4(rgbDataScan2d, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan)
	DATA_SYNCS4(rgbDataScan3d, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS4(rgbDataInfo, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbDataScan2dInfo, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbDataScan3dInfo, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// RGB-only + Odom + User Data
	DATA_SYNCS4(rgbOdomData, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo)
	DATA_SYNCS5(rgbOdomDataScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan)
	DATA_SYNCS5(rgbOdomDataScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS5(rgbOdomDataInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbOdomDataScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbOdomDataScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::Image, sensor_msgs::msg::CameraInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 1 RGBD
	void rgbdCallback(const rtabmap_ros::msg::RGBDImage::ConstSharedPtr);
	DATA_SYNCS2(rgbdScan2d, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS2(rgbdScan3d, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS2(rgbdInfo, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS3(rgbdScan2dInfo, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS3(rgbdScan3dInfo, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 1 RGBD + Odom
	DATA_SYNCS2(rgbdOdom, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS3(rgbdOdomScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS3(rgbdOdomScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS3(rgbdOdomInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(rgbdOdomScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(rgbdOdomScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 1 RGBD + User Data
	DATA_SYNCS2(rgbdData, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS3(rgbdDataScan2d, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS3(rgbdDataScan3d, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS3(rgbdDataInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(rgbdDataScan2dInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(rgbdDataScan3dInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 1 RGBD + Odom + User Data
	DATA_SYNCS3(rgbdOdomData, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS4(rgbdOdomDataScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS4(rgbdOdomDataScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS4(rgbdOdomDataInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbdOdomDataScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbdOdomDataScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 2 RGBD
	DATA_SYNCS2(rgbd2, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS3(rgbd2Scan2d, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS3(rgbd2Scan3d, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS3(rgbd2Info, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(rgbd2Scan2dInfo, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(rgbd2Scan3dInfo, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 2 RGBD + Odom
	DATA_SYNCS3(rgbd2Odom, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS4(rgbd2OdomScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS4(rgbd2OdomScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS4(rgbd2OdomInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbd2OdomScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbd2OdomScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 2 RGBD + User Data
	DATA_SYNCS3(rgbd2Data, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS4(rgbd2DataScan2d, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS4(rgbd2DataScan3d, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS4(rgbd2DataInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbd2DataScan2dInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbd2DataScan3dInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 2 RGBD + Odom + User Data
	DATA_SYNCS4(rgbd2OdomData, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS5(rgbd2OdomDataScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS5(rgbd2OdomDataScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS5(rgbd2OdomDataInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbd2OdomDataScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbd2OdomDataScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 3 RGBD
	DATA_SYNCS3(rgbd3, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS4(rgbd3Scan2d, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS4(rgbd3Scan3d, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS4(rgbd3Info, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbd3Scan2dInfo, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS5(rgbd3Scan3dInfo, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 3 RGBD + Odom
	DATA_SYNCS4(rgbd3Odom, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS5(rgbd3OdomScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS5(rgbd3OdomScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS5(rgbd3OdomInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbd3OdomScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbd3OdomScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 3 RGBD + User Data
	DATA_SYNCS4(rgbd3Data, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS5(rgbd3DataScan2d, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS5(rgbd3DataScan3d, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS5(rgbd3DataInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbd3DataScan2dInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbd3DataScan3dInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 3 RGBD + Odom + User Data
	DATA_SYNCS5(rgbd3OdomData, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS6(rgbd3OdomDataScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS6(rgbd3OdomDataScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS6(rgbd3OdomDataInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS7(rgbd3OdomDataScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS7(rgbd3OdomDataScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 4 RGBD
	DATA_SYNCS4(rgbd4, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS5(rgbd4Scan2d, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS5(rgbd4Scan3d, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS5(rgbd4Info, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbd4Scan2dInfo, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS6(rgbd4Scan3dInfo, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 4 RGBD + Odom
	DATA_SYNCS5(rgbd4Odom, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS6(rgbd4OdomScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS6(rgbd4OdomScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS6(rgbd4OdomInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS7(rgbd4OdomScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS7(rgbd4OdomScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 4 RGBD + User Data
	DATA_SYNCS5(rgbd4Data, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS6(rgbd4DataScan2d, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS6(rgbd4DataScan3d, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS6(rgbd4DataInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS7(rgbd4DataScan2dInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS7(rgbd4DataScan3dInfo, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// 4 RGBD + Odom + User Data
	DATA_SYNCS6(rgbd4OdomData, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage)
	DATA_SYNCS7(rgbd4OdomDataScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan)
	DATA_SYNCS7(rgbd4OdomDataScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS7(rgbd4OdomDataInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS8(rgbd4OdomDataScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS8(rgbd4OdomDataScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, rtabmap_ros::msg::RGBDImage, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// Scan
	void scan2dCallback(const sensor_msgs::msg::LaserScan::ConstSharedPtr);
	void scan3dCallback(const sensor_msgs::msg::PointCloud2::ConstSharedPtr);
	DATA_SYNCS2(scan2dInfo, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS2(scan3dInfo, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// Scan + Odom
	DATA_SYNCS2(odomScan2d, nav_msgs::msg::Odometry, sensor_msgs::msg::LaserScan)
	DATA_SYNCS2(odomScan3d, nav_msgs::msg::Odometry, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS3(odomScan2dInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS3(odomScan3dInfo, nav_msgs::msg::Odometry, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// Scan + User Data
	DATA_SYNCS2(dataScan2d, rtabmap_ros::msg::UserData, sensor_msgs::msg::LaserScan)
	DATA_SYNCS2(dataScan3d, rtabmap_ros::msg::UserData, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS3(dataScan2dInfo, rtabmap_ros::msg::UserData, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS3(dataScan3dInfo, rtabmap_ros::msg::UserData, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// Scan + Odom + User Data
	DATA_SYNCS3(odomDataScan2d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::LaserScan)
	DATA_SYNCS3(odomDataScan3d, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::PointCloud2)
	DATA_SYNCS4(odomDataScan2dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::LaserScan, rtabmap_ros::msg::OdomInfo)
	DATA_SYNCS4(odomDataScan3dInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, sensor_msgs::msg::PointCloud2, rtabmap_ros::msg::OdomInfo)

	// Odom
	void odomCallback(const nav_msgs::msg::Odometry::ConstSharedPtr);
	DATA_SYNCS2(odomInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::OdomInfo)

	// Odom + User Data
	DATA_SYNCS2(odomData, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData)
	DATA_SYNCS3(odomDataInfo, nav_msgs::msg::Odometry, rtabmap_ros::msg::UserData, rtabmap_ros::msg::OdomInfo)
};

} /* namespace rtabmap_ros */

#endif /* INCLUDE_RTABMAP_ROS_COMMONDATASUBSCRIBER_H_ */
