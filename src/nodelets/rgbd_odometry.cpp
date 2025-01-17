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

#include <rtabmap_ros/rgbd_odometry.hpp>

#include <image_geometry/stereo_camera_model.h>

#include <sensor_msgs/image_encodings.hpp>

#include "rtabmap_ros/MsgConversion.h"

#include <rtabmap/core/util3d.h>
#include <rtabmap/core/util2d.h>
#include <rtabmap/utilite/ULogger.h>
#include <rtabmap/utilite/UConversion.h>
#include <rtabmap/utilite/UStl.h>

using namespace rtabmap;

namespace rtabmap_ros
{

RGBDOdometry::RGBDOdometry(const rclcpp::NodeOptions & options) :
		OdometryROS("rgbd_odometry", options),
		approxSync_(0),
		exactSync_(0),
		approxSync2_(0),
		exactSync2_(0),
		approxSync3_(0),
		exactSync3_(0),
		approxSync4_(0),
		exactSync4_(0),
		queueSize_(5)
{
	OdometryROS::init(false, true, false);
}

RGBDOdometry::~RGBDOdometry()
{
	delete approxSync_;
	delete exactSync_;
	delete approxSync2_;
	delete exactSync2_;
	delete approxSync3_;
	delete exactSync3_;
	delete approxSync4_;
	delete exactSync4_;
}

void RGBDOdometry::onOdomInit()
{
	int rgbdCameras = 1;
	bool approxSync = true;
	bool subscribeRGBD = false;
	approxSync = this->declare_parameter("approx_sync", approxSync);
	queueSize_ = this->declare_parameter("queue_size", queueSize_);
	subscribeRGBD = this->declare_parameter("subscribe_rgbd", subscribeRGBD);
	rgbdCameras = this->declare_parameter("rgbd_cameras", rgbdCameras);
	if(rgbdCameras <= 0)
	{
		rgbdCameras = 1;
	}
	if(rgbdCameras > 4)
	{
		RCLCPP_FATAL(this->get_logger(), "Only 4 cameras maximum supported yet.");
	}

	RCLCPP_INFO(this->get_logger(), "RGBDOdometry: approx_sync    = %s", approxSync?"true":"false");
	RCLCPP_INFO(this->get_logger(), "RGBDOdometry: queue_size     = %d", queueSize_);
	RCLCPP_INFO(this->get_logger(), "RGBDOdometry: subscribe_rgbd = %s", subscribeRGBD?"true":"false");
	RCLCPP_INFO(this->get_logger(), "RGBDOdometry: rgbd_cameras   = %d", rgbdCameras);

	std::string subscribedTopicsMsg;
	if(subscribeRGBD)
	{
		if(rgbdCameras >= 2)
		{
			rgbd_image1_sub_.subscribe(this, "rgbd_image0", rmw_qos_profile_sensor_data);
			rgbd_image2_sub_.subscribe(this, "rgbd_image1", rmw_qos_profile_sensor_data);
			if(rgbdCameras >= 3)
			{
				rgbd_image3_sub_.subscribe(this, "rgbd_image2", rmw_qos_profile_sensor_data);
			}
			if(rgbdCameras >= 4)
			{
				rgbd_image4_sub_.subscribe(this, "rgbd_image3", rmw_qos_profile_sensor_data);
			}

			if(rgbdCameras == 2)
			{
				if(approxSync)
				{
					approxSync2_ = new message_filters::Synchronizer<MyApproxSync2Policy>(
							MyApproxSync2Policy(queueSize_),
							rgbd_image1_sub_,
							rgbd_image2_sub_);
					approxSync2_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD2, this, std::placeholders::_1, std::placeholders::_2));
				}
				else
				{
					exactSync2_ = new message_filters::Synchronizer<MyExactSync2Policy>(
							MyExactSync2Policy(queueSize_),
							rgbd_image1_sub_,
							rgbd_image2_sub_);
					exactSync2_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD2, this, std::placeholders::_1, std::placeholders::_2));
				}
				subscribedTopicsMsg = uFormat("\n%s subscribed to (%s sync):\n   %s,\n   %s",
						get_name(),
						approxSync?"approx":"exact",
						rgbd_image1_sub_.getTopic().c_str(),
						rgbd_image2_sub_.getTopic().c_str());
			}
			else if(rgbdCameras == 3)
			{
				if(approxSync)
				{
					approxSync3_ = new message_filters::Synchronizer<MyApproxSync3Policy>(
							MyApproxSync3Policy(queueSize_),
							rgbd_image1_sub_,
							rgbd_image2_sub_,
							rgbd_image3_sub_);
					approxSync3_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD3, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
				}
				else
				{
					exactSync3_ = new message_filters::Synchronizer<MyExactSync3Policy>(
							MyExactSync3Policy(queueSize_),
							rgbd_image1_sub_,
							rgbd_image2_sub_,
							rgbd_image3_sub_);
					exactSync3_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD3, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
				}
				subscribedTopicsMsg = uFormat("\n%s subscribed to (%s sync):\n   %s,\n   %s,\n   %s",
						get_name(),
						approxSync?"approx":"exact",
						rgbd_image1_sub_.getTopic().c_str(),
						rgbd_image2_sub_.getTopic().c_str(),
						rgbd_image3_sub_.getTopic().c_str());
			}
			else if(rgbdCameras == 4)
			{
				if(approxSync)
				{
					approxSync4_ = new message_filters::Synchronizer<MyApproxSync4Policy>(
							MyApproxSync4Policy(queueSize_),
							rgbd_image1_sub_,
							rgbd_image2_sub_,
							rgbd_image3_sub_,
							rgbd_image4_sub_);
					approxSync4_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD4, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
				}
				else
				{
					exactSync4_ = new message_filters::Synchronizer<MyExactSync4Policy>(
							MyExactSync4Policy(queueSize_),
							rgbd_image1_sub_,
							rgbd_image2_sub_,
							rgbd_image3_sub_,
							rgbd_image4_sub_);
					exactSync4_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD4, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
				}
				subscribedTopicsMsg = uFormat("\n%s subscribed to (%s sync):\n   %s,\n   %s,\n   %s,\n   %s",
						get_name(),
						approxSync?"approx":"exact",
						rgbd_image1_sub_.getTopic().c_str(),
						rgbd_image2_sub_.getTopic().c_str(),
						rgbd_image3_sub_.getTopic().c_str(),
						rgbd_image4_sub_.getTopic().c_str());
			}
		}
		else
		{
			rgbdSub_ = create_subscription<rtabmap_ros::msg::RGBDImage>("rgbd_image", rclcpp::SensorDataQoS(), std::bind(&RGBDOdometry::callbackRGBD, this, std::placeholders::_1));

			subscribedTopicsMsg =
					uFormat("\n%s subscribed to:\n   %s",
					get_name(),
					rgbdSub_->get_topic_name());
		}
	}
	else
	{
		image_transport::TransportHints hints(this);
		image_mono_sub_.subscribe(this, "rgb/image", hints.getTransport(), rmw_qos_profile_sensor_data);
		image_depth_sub_.subscribe(this, "depth/image", hints.getTransport(), rmw_qos_profile_sensor_data);
		info_sub_.subscribe(this, "rgb/camera_info", rmw_qos_profile_sensor_data);

		if(approxSync)
		{
			approxSync_ = new message_filters::Synchronizer<MyApproxSyncPolicy>(MyApproxSyncPolicy(queueSize_), image_mono_sub_, image_depth_sub_, info_sub_);
			approxSync_->registerCallback(std::bind(&RGBDOdometry::callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		}
		else
		{
			exactSync_ = new message_filters::Synchronizer<MyExactSyncPolicy>(MyExactSyncPolicy(queueSize_), image_mono_sub_, image_depth_sub_, info_sub_);
			exactSync_->registerCallback(std::bind(&RGBDOdometry::callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		}

		subscribedTopicsMsg = uFormat("\n%s subscribed to (%s sync):\n   %s,\n   %s,\n   %s",
				get_name(),
				approxSync?"approx":"exact",
				image_mono_sub_.getTopic().c_str(),
				image_depth_sub_.getTopic().c_str(),
				info_sub_.getTopic().c_str());
	}
	this->startWarningThread(subscribedTopicsMsg, approxSync);
}

void RGBDOdometry::updateParameters(ParametersMap & parameters)
{
	//make sure we are using Reg/Strategy=0
	ParametersMap::iterator iter = parameters.find(Parameters::kRegStrategy());
	if(iter != parameters.end() && iter->second.compare("0") != 0)
	{
		RCLCPP_WARN(this->get_logger(), "RGBD odometry works only with \"Reg/Strategy\"=0. Ignoring value %s.", iter->second.c_str());
	}
	uInsert(parameters, ParametersPair(Parameters::kRegStrategy(), "0"));

	int estimationType = Parameters::defaultVisEstimationType();
	Parameters::parse(parameters, Parameters::kVisEstimationType(), estimationType);
	int rgbdCameras = 1;
	bool subscribeRGBD = false;
	this->get_parameter("subscribe_rgbd", subscribeRGBD);
	this->get_parameter("rgbd_cameras", rgbdCameras);
	if(subscribeRGBD && rgbdCameras> 1 && estimationType>0)
	{
		RCLCPP_WARN(this->get_logger(), "Setting \"%s\" parameter to 0 (%d is not supported "
				"for multi-cameras) as \"subscribe_rgbd\" is "
				"true and \"rgbd_cameras\">1. Set \"%s\" to 0 to suppress this warning.",
				Parameters::kVisEstimationType().c_str(),
				estimationType,
				Parameters::kVisEstimationType().c_str());
		uInsert(parameters, ParametersPair(Parameters::kVisEstimationType(), "0"));
	}
}

void RGBDOdometry::commonCallback(
			const std::vector<cv_bridge::CvImageConstPtr> & rgbImages,
			const std::vector<cv_bridge::CvImageConstPtr> & depthImages,
			const std::vector<sensor_msgs::msg::CameraInfo>& cameraInfos)
{
	UASSERT(rgbImages.size() > 0 && rgbImages.size() == depthImages.size() && rgbImages.size() == cameraInfos.size());
	rclcpp::Time higherStamp;
	int imageWidth = rgbImages[0]->image.cols;
	int imageHeight = rgbImages[0]->image.rows;
	int depthWidth = depthImages[0]->image.cols;
	int depthHeight = depthImages[0]->image.rows;

	UASSERT_MSG(
		imageWidth % depthWidth == 0 && imageHeight % depthHeight == 0 &&
		imageWidth/depthWidth == imageHeight/depthHeight,
		uFormat("rgb=%dx%d depth=%dx%d", imageWidth, imageHeight, depthWidth, depthHeight).c_str());

	int cameraCount = rgbImages.size();
	cv::Mat rgb;
	cv::Mat depth;
	pcl::PointCloud<pcl::PointXYZ> scanCloud;
	std::vector<CameraModel> cameraModels;
	for(unsigned int i=0; i<rgbImages.size(); ++i)
	{
		if(!(rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::TYPE_8UC1) ==0 ||
			 rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::MONO8) ==0 ||
			 rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::MONO16) ==0 ||
			 rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::BGR8) == 0 ||
			 rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::RGB8) == 0 ||
			 rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::BGRA8) == 0 ||
			 rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::RGBA8) == 0 ||
			 rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::BAYER_GRBG8) == 0) ||
			!(depthImages[i]->encoding.compare(sensor_msgs::image_encodings::TYPE_16UC1) == 0 ||
			 depthImages[i]->encoding.compare(sensor_msgs::image_encodings::TYPE_32FC1) == 0 ||
			 depthImages[i]->encoding.compare(sensor_msgs::image_encodings::MONO16) == 0))
			 {
				RCLCPP_ERROR(this->get_logger(), "Input type must be image=mono8,mono16,rgb8,bgr8,bgra8,rgba8 and "
				"image_depth=32FC1,16UC1,mono16. Current rgb=%s and depth=%s",
					rgbImages[i]->encoding.c_str(),
					depthImages[i]->encoding.c_str());
				return;
			}
		UASSERT_MSG(rgbImages[i]->image.cols == imageWidth && rgbImages[i]->image.rows == imageHeight,
				uFormat("imageWidth=%d vs %d imageHeight=%d vs %d",
						imageWidth,
						rgbImages[i]->image.cols,
						imageHeight,
						rgbImages[i]->image.rows).c_str());
		UASSERT_MSG(depthImages[i]->image.cols == depthWidth && depthImages[i]->image.rows == depthHeight,
				uFormat("depthWidth=%d vs %d depthHeight=%d vs %d",
						depthWidth,
						depthImages[i]->image.cols,
						depthHeight,
						depthImages[i]->image.rows).c_str());

		rclcpp::Time stamp = timestampFromROS(rgbImages[i]->header.stamp)>timestampFromROS(depthImages[i]->header.stamp)?rgbImages[i]->header.stamp:depthImages[i]->header.stamp;

		if(i == 0)
		{
			higherStamp = stamp;
		}
		else if(stamp > higherStamp)
		{
			higherStamp = stamp;
		}

		Transform localTransform = getTransform(this->frameId(), rgbImages[i]->header.frame_id, stamp, tfBuffer(), waitForTransform());
		if(localTransform.isNull())
		{
			return;
		}

		cv_bridge::CvImageConstPtr ptrImage = rgbImages[i];
		if(rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::TYPE_8UC1) !=0 &&
		   rgbImages[i]->encoding.compare(sensor_msgs::image_encodings::MONO8) != 0)
		{
			ptrImage = cv_bridge::cvtColor(rgbImages[i], "mono8");
		}

		cv_bridge::CvImageConstPtr ptrDepth = depthImages[i];
		cv::Mat subDepth = ptrDepth->image;

		// initialize
		if(rgb.empty())
		{
			rgb = cv::Mat(imageHeight, imageWidth*cameraCount, ptrImage->image.type());
		}
		if(depth.empty())
		{
			depth = cv::Mat(depthHeight, depthWidth*cameraCount, subDepth.type());
		}

		if(ptrImage->image.type() == rgb.type())
		{
			ptrImage->image.copyTo(cv::Mat(rgb, cv::Rect(i*imageWidth, 0, imageWidth, imageHeight)));
		}
		else
		{
			RCLCPP_ERROR(this->get_logger(), "Some RGB images are not the same type!");
			return;
		}

		if(subDepth.type() == depth.type())
		{
			subDepth.copyTo(cv::Mat(depth, cv::Rect(i*depthWidth, 0, depthWidth, depthHeight)));
		}
		else
		{
			RCLCPP_ERROR(this->get_logger(), "Some Depth images are not the same type! %d vs %d", subDepth.type(), depth.type());
			return;
		}

		cameraModels.push_back(rtabmap_ros::cameraModelFromROS(cameraInfos[i], localTransform));
	}

	rtabmap::SensorData data(
			rgb,
			depth,
			cameraModels,
			0,
			rtabmap_ros::timestampFromROS(higherStamp));

	this->processData(data, higherStamp);
}

void RGBDOdometry::callback(
		const sensor_msgs::msg::Image::ConstSharedPtr image,
		const sensor_msgs::msg::Image::ConstSharedPtr depth,
		const sensor_msgs::msg::CameraInfo::ConstSharedPtr cameraInfo)
{
	callbackCalled();
	if(!this->isPaused())
	{
		std::vector<cv_bridge::CvImageConstPtr> imageMsgs(1);
		std::vector<cv_bridge::CvImageConstPtr> depthMsgs(1);
		std::vector<sensor_msgs::msg::CameraInfo> infoMsgs;
		imageMsgs[0] = cv_bridge::toCvShare(image);
		depthMsgs[0] = cv_bridge::toCvShare(depth);
		infoMsgs.push_back(*cameraInfo);

		this->commonCallback(imageMsgs, depthMsgs, infoMsgs);
	}
}

void RGBDOdometry::callbackRGBD(
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image)
{
	callbackCalled();
	if(!this->isPaused())
	{
		std::vector<cv_bridge::CvImageConstPtr> imageMsgs(1);
		std::vector<cv_bridge::CvImageConstPtr> depthMsgs(1);
		std::vector<sensor_msgs::msg::CameraInfo> infoMsgs;
		rtabmap_ros::toCvShare(image, imageMsgs[0], depthMsgs[0]);
		infoMsgs.push_back(image->rgb_camera_info);

		this->commonCallback(imageMsgs, depthMsgs, infoMsgs);
	}
}

void RGBDOdometry::callbackRGBD2(
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image,
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image2)
{
	callbackCalled();
	if(!this->isPaused())
	{
		std::vector<cv_bridge::CvImageConstPtr> imageMsgs(2);
		std::vector<cv_bridge::CvImageConstPtr> depthMsgs(2);
		std::vector<sensor_msgs::msg::CameraInfo> infoMsgs;
		rtabmap_ros::toCvShare(image, imageMsgs[0], depthMsgs[0]);
		rtabmap_ros::toCvShare(image2, imageMsgs[1], depthMsgs[1]);
		infoMsgs.push_back(image->rgb_camera_info);
		infoMsgs.push_back(image2->rgb_camera_info);

		this->commonCallback(imageMsgs, depthMsgs, infoMsgs);
	}
}

void RGBDOdometry::callbackRGBD3(
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image,
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image2,
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image3)
{
	callbackCalled();
	if(!this->isPaused())
	{
		std::vector<cv_bridge::CvImageConstPtr> imageMsgs(3);
		std::vector<cv_bridge::CvImageConstPtr> depthMsgs(3);
		std::vector<sensor_msgs::msg::CameraInfo> infoMsgs;
		rtabmap_ros::toCvShare(image, imageMsgs[0], depthMsgs[0]);
		rtabmap_ros::toCvShare(image2, imageMsgs[1], depthMsgs[1]);
		rtabmap_ros::toCvShare(image3, imageMsgs[2], depthMsgs[2]);
		infoMsgs.push_back(image->rgb_camera_info);
		infoMsgs.push_back(image2->rgb_camera_info);
		infoMsgs.push_back(image3->rgb_camera_info);

		this->commonCallback(imageMsgs, depthMsgs, infoMsgs);
	}
}

void RGBDOdometry::callbackRGBD4(
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image,
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image2,
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image3,
		const rtabmap_ros::msg::RGBDImage::ConstSharedPtr image4)
{
	callbackCalled();
	if(!this->isPaused())
	{
		std::vector<cv_bridge::CvImageConstPtr> imageMsgs(4);
		std::vector<cv_bridge::CvImageConstPtr> depthMsgs(4);
		std::vector<sensor_msgs::msg::CameraInfo> infoMsgs;
		rtabmap_ros::toCvShare(image, imageMsgs[0], depthMsgs[0]);
		rtabmap_ros::toCvShare(image2, imageMsgs[1], depthMsgs[1]);
		rtabmap_ros::toCvShare(image3, imageMsgs[2], depthMsgs[2]);
		rtabmap_ros::toCvShare(image4, imageMsgs[3], depthMsgs[3]);
		infoMsgs.push_back(image->rgb_camera_info);
		infoMsgs.push_back(image2->rgb_camera_info);
		infoMsgs.push_back(image3->rgb_camera_info);
		infoMsgs.push_back(image4->rgb_camera_info);

		this->commonCallback(imageMsgs, depthMsgs, infoMsgs);
	}
}

void RGBDOdometry::flushCallbacks()
{
	// flush callbacks
	if(approxSync_)
	{
		delete approxSync_;
		approxSync_ = new message_filters::Synchronizer<MyApproxSyncPolicy>(MyApproxSyncPolicy(queueSize_), image_mono_sub_, image_depth_sub_, info_sub_);
		approxSync_->registerCallback(std::bind(&RGBDOdometry::callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	if(exactSync_)
	{
		delete exactSync_;
		exactSync_ = new message_filters::Synchronizer<MyExactSyncPolicy>(MyExactSyncPolicy(queueSize_), image_mono_sub_, image_depth_sub_, info_sub_);
		exactSync_->registerCallback(std::bind(&RGBDOdometry::callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	if(approxSync2_)
	{
		delete approxSync2_;
		approxSync2_ = new message_filters::Synchronizer<MyApproxSync2Policy>(
				MyApproxSync2Policy(queueSize_),
				rgbd_image1_sub_,
				rgbd_image2_sub_);
		approxSync2_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD2, this, std::placeholders::_1, std::placeholders::_2));
	}
	if(exactSync2_)
	{
		delete exactSync2_;
		exactSync2_ = new message_filters::Synchronizer<MyExactSync2Policy>(
				MyExactSync2Policy(queueSize_),
				rgbd_image1_sub_,
				rgbd_image2_sub_);
		exactSync2_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD2, this, std::placeholders::_1, std::placeholders::_2));
	}
	if(approxSync3_)
	{
		delete approxSync3_;
		approxSync3_ = new message_filters::Synchronizer<MyApproxSync3Policy>(
				MyApproxSync3Policy(queueSize_),
				rgbd_image1_sub_,
				rgbd_image2_sub_,
				rgbd_image3_sub_);
		approxSync3_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD3, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	if(exactSync3_)
	{
		delete exactSync3_;
		exactSync3_ = new message_filters::Synchronizer<MyExactSync3Policy>(
				MyExactSync3Policy(queueSize_),
				rgbd_image1_sub_,
				rgbd_image2_sub_,
				rgbd_image3_sub_);
		exactSync3_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD3, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	if(approxSync4_)
	{
		delete approxSync4_;
		approxSync4_ = new message_filters::Synchronizer<MyApproxSync4Policy>(
				MyApproxSync4Policy(queueSize_),
				rgbd_image1_sub_,
				rgbd_image2_sub_,
				rgbd_image3_sub_,
				rgbd_image4_sub_);
		approxSync4_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD4, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	}
	if(exactSync4_)
	{
		delete exactSync4_;
		exactSync4_ = new message_filters::Synchronizer<MyExactSync4Policy>(
				MyExactSync4Policy(queueSize_),
				rgbd_image1_sub_,
				rgbd_image2_sub_,
				rgbd_image3_sub_,
				rgbd_image4_sub_);
		exactSync4_->registerCallback(std::bind(&RGBDOdometry::callbackRGBD4, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	}
}

}

#include "rclcpp_components/register_node_macro.hpp"

// Register the component with class_loader.
// This acts as a sort of entry point, allowing the component to be discoverable when its library
// is being loaded into a running process.
RCLCPP_COMPONENTS_REGISTER_NODE(rtabmap_ros::RGBDOdometry)
