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

#include "rtabmap_ros/OdometryROS.h"

#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <nav_msgs/msg/odometry.hpp>

#include <pcl_conversions/pcl_conversions.h>

#include <cv_bridge/cv_bridge.h>

#include <rtabmap/core/odometry/OdometryF2M.h>
#include <rtabmap/core/odometry/OdometryF2F.h>
#include <rtabmap/core/util3d.h>
#include <rtabmap/core/util3d_transforms.h>
#include <rtabmap/core/Memory.h>
#include <rtabmap/core/Signature.h>
#include "rtabmap_ros/MsgConversion.h"
#include "rtabmap_ros/msg/odom_info.hpp"
#include "rtabmap/utilite/UConversion.h"
#include "rtabmap/utilite/ULogger.h"
#include "rtabmap/utilite/UStl.h"
#include "rtabmap/utilite/UFile.h"
#include "rtabmap/utilite/UMath.h"

#define BAD_COVARIANCE 9999

using namespace rtabmap;

namespace rtabmap_ros {

OdometryROS::OdometryROS(const rclcpp::NodeOptions & options) :
		OdometryROS("odometry", options)
	{}

OdometryROS::OdometryROS(const std::string & name, const rclcpp::NodeOptions & options) :
	Node(name, options),
	odometry_(0),
	warningThread_(0),
	callbackCalled_(false),
	frameId_("base_link"),
	odomFrameId_("odom"),
	groundTruthFrameId_(""),
	groundTruthBaseFrameId_(""),
	guessFrameId_(""),
	guessMinTranslation_(0.0),
	guessMinRotation_(0.0),
	guessMinTime_(0.0),
	publishTf_(true),
	waitForTransform_(0.1), // 100 ms
	publishNullWhenLost_(true),
	paused_(false),
	resetCountdown_(0),
	resetCurrentCount_(0),
	previousStamp_(0.0),
	expectedUpdateRate_(0.0),
	odomStrategy_(Parameters::defaultOdomStrategy()),
	waitIMUToinit_(false),
	imuProcessed_(false),
	lastImuReceivedStamp_(0.0),
	configPath_(),
	initialPose_(Transform::getIdentity())
{
	odomPub_ = create_publisher<nav_msgs::msg::Odometry>("odom", 1);
	odomInfoPub_ = create_publisher<rtabmap_ros::msg::OdomInfo>("odom_info", 1);
	odomLocalMap_ = create_publisher<sensor_msgs::msg::PointCloud2>("odom_local_map", 1);
	odomLocalScanMap_ = create_publisher<sensor_msgs::msg::PointCloud2>("odom_local_scan_map", 1);
	odomLastFrame_ = create_publisher<sensor_msgs::msg::PointCloud2>("odom_last_frame", 1);

	tfBuffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
	//auto timer_interface = std::make_shared<tf2_ros::CreateTimerROS>(
	//	this->get_node_base_interface(),
	//	this->get_node_timers_interface());
	//tfBuffer_->setCreateTimerInterface(timer_interface);
	tfListener_ = std::make_shared<tf2_ros::TransformListener>(*tfBuffer_);
	tfBroadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);

	std::string initialPoseStr;
	frameId_ = this->declare_parameter("frame_id", frameId_);
	odomFrameId_ = this->declare_parameter("odom_frame_id", odomFrameId_);
	publishTf_ = this->declare_parameter("publish_tf", publishTf_);

	waitForTransform_ = this->declare_parameter("wait_for_transform", waitForTransform_);
	initialPoseStr = this->declare_parameter("initial_pose", initialPoseStr); // "x y z roll pitch yaw"
	groundTruthFrameId_ = this->declare_parameter("ground_truth_frame_id", groundTruthFrameId_);
	groundTruthBaseFrameId_ = this->declare_parameter("ground_truth_base_frame_id", frameId_);
	configPath_ = this->declare_parameter("config_path", configPath_);
	publishNullWhenLost_ = this->declare_parameter("publish_null_when_lost", publishNullWhenLost_);

	guessFrameId_ = this->declare_parameter("guess_frame_id", guessFrameId_);
	guessMinTranslation_ = this->declare_parameter("guess_min_translation", guessMinTranslation_);
	guessMinRotation_ = this->declare_parameter("guess_min_rotation", guessMinRotation_);
	guessMinTime_ = this->declare_parameter("guess_min_time", guessMinTime_);

	expectedUpdateRate_ = this->declare_parameter("expected_update_rate", expectedUpdateRate_);

	waitIMUToinit_ = this->declare_parameter("wait_imu_to_init", waitIMUToinit_);

	if(publishTf_ && !guessFrameId_.empty() && guessFrameId_.compare(odomFrameId_) == 0)
	{
		RCLCPP_WARN(this->get_logger(), "\"publish_tf\" and \"guess_frame_id\" cannot be used "
				"at the same time if \"guess_frame_id\" and \"odom_frame_id\" "
				"are the same frame (value=\"%s\"). \"guess_frame_id\" is disabled.", odomFrameId_.c_str());
		guessFrameId_.clear();
	}
	RCLCPP_INFO(this->get_logger(), "Odometry: frame_id               = %s", frameId_.c_str());
	RCLCPP_INFO(this->get_logger(), "Odometry: odom_frame_id          = %s", odomFrameId_.c_str());
	RCLCPP_INFO(this->get_logger(), "Odometry: publish_tf             = %s", publishTf_?"true":"false");
	RCLCPP_INFO(this->get_logger(), "Odometry: wait_for_transform     = %f", waitForTransform_);
	RCLCPP_INFO(this->get_logger(), "Odometry: initial_pose           = %s", initialPose_.prettyPrint().c_str());
	RCLCPP_INFO(this->get_logger(), "Odometry: ground_truth_frame_id  = %s", groundTruthFrameId_.c_str());
	RCLCPP_INFO(this->get_logger(), "Odometry: ground_truth_base_frame_id = %s", groundTruthBaseFrameId_.c_str());
	RCLCPP_INFO(this->get_logger(), "Odometry: config_path            = %s", configPath_.c_str());
	RCLCPP_INFO(this->get_logger(), "Odometry: publish_null_when_lost = %s", publishNullWhenLost_?"true":"false");
	RCLCPP_INFO(this->get_logger(), "Odometry: guess_frame_id         = %s", guessFrameId_.c_str());
	RCLCPP_INFO(this->get_logger(), "Odometry: guess_min_translation  = %f", guessMinTranslation_);
	RCLCPP_INFO(this->get_logger(), "Odometry: guess_min_rotation     = %f", guessMinRotation_);
	RCLCPP_INFO(this->get_logger(), "Odometry: guess_min_time         = %f", guessMinTime_);
	RCLCPP_INFO(this->get_logger(), "Odometry: expected_update_rate   = %f Hz", expectedUpdateRate_);
	RCLCPP_INFO(this->get_logger(), "Odometry: wait_imu_to_init       = %s", waitIMUToinit_?"true":"false");

	configPath_ = uReplaceChar(configPath_, '~', UDirectory::homeDir());
	if(configPath_.size() && configPath_.at(0) != '/')
	{
		configPath_ = UDirectory::currentDir(true) + configPath_;
	}

	if(initialPoseStr.size())
	{
		std::vector<std::string> values = uListToVector(uSplit(initialPoseStr, ' '));
		if(values.size() == 6)
		{
			initialPose_ = Transform(
					uStr2Float(values[0]), uStr2Float(values[1]), uStr2Float(values[2]),
					uStr2Float(values[3]), uStr2Float(values[4]), uStr2Float(values[5]));
		}
		else
		{
			RCLCPP_ERROR(this->get_logger(), "Wrong initial_pose format: %s (should be \"x y z roll pitch yaw\" with angle in radians). "
					  "Identity will be used...", initialPoseStr.c_str());
		}
	}
}

OdometryROS::~OdometryROS()
{
	if(warningThread_)
	{
		callbackCalled();
		warningThread_->join();
		delete warningThread_;
	}

	delete odometry_;
}

void OdometryROS::init(bool stereoParams, bool visParams, bool icpParams)
{
	stereoParams_ = stereoParams;
	visParams_ = visParams;
	icpParams_ = icpParams;

	//parameters
	parameters_ = Parameters::getDefaultOdometryParameters(stereoParams_, visParams_, icpParams_);
	if(icpParams_)
	{
		if(!visParams_)
		{
			uInsert(parameters_, ParametersPair(Parameters::kRegStrategy(), "1"));
		}
		else
		{
			uInsert(parameters_, ParametersPair(Parameters::kRegStrategy(), "2"));
		}
	}
	parameters_.insert(*Parameters::getDefaultParameters().find(Parameters::kRtabmapImagesAlreadyRectified()));
	if(!configPath_.empty())
	{
		if(UFile::exists(configPath_.c_str()))
		{
			RCLCPP_INFO(this->get_logger(), "Odometry: Loading parameters from %s", configPath_.c_str());
			rtabmap::ParametersMap allParameters;
			Parameters::readINI(configPath_.c_str(), allParameters);
			// only update odometry parameters
			for(ParametersMap::iterator iter=parameters_.begin(); iter!=parameters_.end(); ++iter)
			{
				ParametersMap::iterator jter = allParameters.find(iter->first);
				if(jter!=allParameters.end())
				{
					iter->second = jter->second;
				}
			}
		}
		else
		{
			RCLCPP_ERROR(this->get_logger(), "Config file \"%s\" not found!", configPath_.c_str());
		}
	}

	for(rtabmap::ParametersMap::iterator iter=parameters_.begin(); iter!=parameters_.end(); ++iter)
	{
		rclcpp::Parameter parameter;
		std::string vStr = this->declare_parameter(iter->first, iter->second); 
	 	if(vStr.compare(iter->second)!=0)
		{
			RCLCPP_INFO(this->get_logger(), "Setting odometry parameter \"%s\"=\"%s\"", iter->first.c_str(), vStr.c_str());
			iter->second = vStr;
		}

		if(iter->first.compare(Parameters::kVisMinInliers()) == 0 && atoi(iter->second.c_str()) < 8)
		{
			RCLCPP_WARN(this->get_logger(), "Parameter min_inliers must be >= 8, setting to 8...");
			iter->second = uNumber2Str(8);
		}
	}

	std::vector<std::string> argList = this->get_node_options().arguments();
	char ** argv = new char*[argList.size()];
	for(unsigned int i=0; i<argList.size(); ++i)
	{
		argv[i] = &argList[i].at(0);
	}

	rtabmap::ParametersMap parameters = rtabmap::Parameters::parseArguments(argList.size(), argv);
	delete[] argv;
	for(rtabmap::ParametersMap::iterator iter=parameters.begin(); iter!=parameters.end(); ++iter)
	{
		rtabmap::ParametersMap::iterator jter = parameters_.find(iter->first);
		if(jter!=parameters_.end())
		{
			RCLCPP_INFO(this->get_logger(), "Update odometry parameter \"%s\"=\"%s\" from arguments", iter->first.c_str(), iter->second.c_str());
			jter->second = iter->second;
		}
	}

	// Backward compatibility
	for(std::map<std::string, std::pair<bool, std::string> >::const_iterator iter=Parameters::getRemovedParameters().begin();
		iter!=Parameters::getRemovedParameters().end();
		++iter)
	{
		rclcpp::Parameter parameter;
		if(get_parameter(iter->first, parameter))
		{
			std::string vStr = parameter.as_string();
			if(iter->second.first && parameters_.find(iter->second.second) != parameters_.end())
			{
				// can be migrated
				parameters_.at(iter->second.second)= vStr;
				RCLCPP_WARN(this->get_logger(), "Odometry: Parameter name changed: \"%s\" -> \"%s\". Please update your launch file accordingly. Value \"%s\" is still set to the new parameter name.",
						iter->first.c_str(), iter->second.second.c_str(), vStr.c_str());
			}
			else
			{
				if(iter->second.second.empty())
				{
					RCLCPP_ERROR(this->get_logger(), "Odometry: Parameter \"%s\" doesn't exist anymore!",
							iter->first.c_str());
				}
				else
				{
					RCLCPP_ERROR(this->get_logger(), "Odometry: Parameter \"%s\" doesn't exist anymore! You may look at this similar parameter: \"%s\"",
							iter->first.c_str(), iter->second.second.c_str());
				}
			}
		}
	}

	Parameters::parse(parameters_, Parameters::kOdomResetCountdown(), resetCountdown_);
	parameters_.at(Parameters::kOdomResetCountdown()) = "0"; // use modified reset countdown here

	this->updateParameters(parameters_);

	odometry_ = Odometry::create(parameters_);
	if(!initialPose_.isIdentity())
	{
		odometry_->reset(initialPose_);
	}

	resetSrv_ = this->create_service<std_srvs::srv::Empty>("reset_odom", std::bind(&OdometryROS::resetOdom, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	resetToPoseSrv_ = this->create_service<rtabmap_ros::srv::ResetPose>("reset_odom_to_pose", std::bind(&OdometryROS::resetToPose, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	pauseSrv_ = this->create_service<std_srvs::srv::Empty>("pause_odom", std::bind(&OdometryROS::pause, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	resumeSrv_ = this->create_service<std_srvs::srv::Empty>("resume_odom", std::bind(&OdometryROS::resume, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	setLogDebugSrv_ = this->create_service<std_srvs::srv::Empty>("log_debug", std::bind(&OdometryROS::setLogDebug, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	setLogInfoSrv_ = this->create_service<std_srvs::srv::Empty>("log_info", std::bind(&OdometryROS::setLogInfo, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	setLogWarnSrv_ = this->create_service<std_srvs::srv::Empty>("log_warning", std::bind(&OdometryROS::setLogWarn, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	setLogErrorSrv_ = this->create_service<std_srvs::srv::Empty>("log_error", std::bind(&OdometryROS::setLogError, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	odomStrategy_ = 0;
	Parameters::parse(this->parameters(), Parameters::kOdomStrategy(), odomStrategy_);
	if(waitIMUToinit_ || odometry_->canProcessAsyncIMU())
	{
		int queueSize = 10;
		queueSize = this->declare_parameter("queue_size", queueSize);
		imuSub_ = create_subscription<sensor_msgs::msg::Imu>("imu", queueSize*5, std::bind(&OdometryROS::callbackIMU, this, std::placeholders::_1));
		RCLCPP_INFO(this->get_logger(), "odometry: Subscribing to IMU topic %s", imuSub_->get_topic_name());
	}

	onOdomInit();
}

void OdometryROS::startWarningThread(const std::string & subscribedTopicsMsg, bool approxSync)
{
	RCLCPP_INFO(this->get_logger(), "%s", subscribedTopicsMsg.c_str());

	subscribedTopicsMsg_ = subscribedTopicsMsg;
	warningThread_ = new std::thread([&](){
		rclcpp::Rate r(1.0/5.0);
		while(!callbackCalled_)
		{
			r.sleep();
			if(!callbackCalled_)
			{
				RCLCPP_WARN(this->get_logger(), "%s: Did not receive data since 5 seconds! Make sure the input topics are "
						"published (\"$ rostopic hz my_topic\") and the timestamps in their "
						"header are set. %s%s",
						this->get_name(),
						approxSync?"":"Parameter \"approx_sync\" is false, which means that input "
							"topics should have all the exact timestamp for the callback to be called.",
							subscribedTopicsMsg_.c_str());
			}
		}
	});
}

void OdometryROS::callbackIMU(const sensor_msgs::msg::Imu::SharedPtr msg)
{
	if(!this->isPaused())
	{
		if(!odometry_->canProcessAsyncIMU() &&
		   !odometry_->getPose().isIdentity())
		{
			// For non-inertial odometry approaches, IMU is only used to initialize the initial orientation below
			return;
		}

		double stamp = timestampFromROS(msg->header.stamp);
		rtabmap::Transform localTransform = rtabmap::Transform::getIdentity();
		if(this->frameId().compare(msg->header.frame_id) != 0)
		{
			localTransform = getTransform(this->frameId(), msg->header.frame_id, msg->header.stamp, *tfBuffer_, waitForTransform_);
		}
		if(localTransform.isNull())
		{
			RCLCPP_ERROR(this->get_logger(), "Could not transform IMU msg from frame \"%s\" to frame \"%s\", TF not available at time %f",
					msg->header.frame_id.c_str(), this->frameId().c_str(), stamp);
			return;
		}

		IMU imu(cv::Vec4d(msg->orientation.x, msg->orientation.y, msg->orientation.z, msg->orientation.w),
				cv::Mat(3,3,CV_64FC1,(void*)msg->orientation_covariance.data()).clone(),
				cv::Vec3d(msg->angular_velocity.x, msg->angular_velocity.y, msg->angular_velocity.z),
				cv::Mat(3,3,CV_64FC1,(void*)msg->angular_velocity_covariance.data()).clone(),
				cv::Vec3d(msg->linear_acceleration.x, msg->linear_acceleration.y, msg->linear_acceleration.z),
				cv::Mat(3,3,CV_64FC1,(void*)msg->linear_acceleration_covariance.data()).clone(),
				localTransform);

		if(!odometry_->canProcessAsyncIMU())
		{
			if(!odometry_->getPose().isIdentity())
			{
				// For these approaches, IMU is only used to initialize the initial orientation
				return;
			}

			// align with gravity
			if(!imu.localTransform().isNull())
			{
				if(imu.orientation()[0] != 0 || imu.orientation()[1] != 0 || imu.orientation()[2] != 0 || imu.orientation()[3] != 0)
				{
					Transform rotation(0,0,0, imu.orientation()[0], imu.orientation()[1], imu.orientation()[2], imu.orientation()[3]);
					rotation = rotation * imu.localTransform().rotation().inverse();
					this->reset(rotation);
					float r,p,y;
					rotation.getEulerAngles(r,p,y);
					RCLCPP_WARN(this->get_logger(), "odometry: Initialized odometry with IMU's orientation (rpy = %f %f %f).", r,p,y);
				}
				else if(imu.linearAcceleration()[0]!=0.0 &&
					imu.linearAcceleration()[1]!=0.0 &&
					imu.linearAcceleration()[2]!=0.0 &&
					!imu.localTransform().isNull())
				{
					Eigen::Vector3f n(imu.linearAcceleration()[0], imu.linearAcceleration()[1], imu.linearAcceleration()[2]);
					n = imu.localTransform().rotation().toEigen3f() * n;
					n.normalize();
					Eigen::Vector3f z(0,0,1);
					//get rotation from z to n;
					Eigen::Matrix3f R;
					R = Eigen::Quaternionf().setFromTwoVectors(n,z);
					Transform rotation(
							R(0,0), R(0,1), R(0,2), 0,
							R(1,0), R(1,1), R(1,2), 0,
							R(2,0), R(2,1), R(2,2), 0);
					this->reset(rotation);
					float r,p,y;
					rotation.getEulerAngles(r,p,y);
					RCLCPP_WARN(this->get_logger(), "odometry: Initialized odometry with IMU's accelerometer (rpy = %f %f %f).", r,p,y);
				}
			}
		}
		else
		{
			SensorData data(imu, 0, stamp);
			this->processData(data, msg->header.stamp);
			imuProcessed_ = true;
			lastImuReceivedStamp_ = stamp;

			if(bufferedData_.isValid() && stamp >= bufferedData_.stamp())
			{
				processData(bufferedData_, rclcpp::Time(bufferedData_.stamp()*10e9));
			}
			bufferedData_ = SensorData();
		}
	}
}

void OdometryROS::processData(const SensorData & data, const rclcpp::Time & stamp)
{
	if((waitIMUToinit_ && !imuProcessed_) && odometry_->framesProcessed() == 0 && odometry_->getPose().isIdentity() && data.imu().empty())
	{
		RCLCPP_WARN(this->get_logger(), "odometry: waiting imu to initialize orientation (wait_imu_to_init=true)");
		return;
	}

	Transform groundTruth;
	if(!data.imageRaw().empty() || !data.laserScanRaw().isEmpty())
	{
		if(odometry_->canProcessAsyncIMU() && data.imu().empty() && lastImuReceivedStamp_>0.0 && data.stamp() > lastImuReceivedStamp_)
		{
			//RCLCPP_WARN(this->get_logger(), "Data received is more recent than last imu received, waiting for imu update to process it.");
			if(bufferedData_.isValid())
			{
				RCLCPP_ERROR(this->get_logger(), "Overwriting previous data! Make sure IMU is published faster than data rate.");
			}
			bufferedData_ = data;
			return;
		}

		if(previousStamp_>0.0 && previousStamp_ >= stamp.seconds())
		{
			RCLCPP_WARN(this->get_logger(), "Odometry: Detected not valid consecutive stamps (previous=%fs new=%fs). New stamp should be always greater than previous stamp. This new data is ignored. This message will appear only once.",
					previousStamp_, stamp.seconds());
			return;
		}
		else if(expectedUpdateRate_ > 0 &&
		  previousStamp_ > 0 &&
		  (stamp.seconds()-previousStamp_) < 1.0/expectedUpdateRate_)
		{
			RCLCPP_WARN(this->get_logger(), "Odometry: Aborting odometry update, higher frame rate detected (%f Hz) than the expected one (%f Hz). (stamps: previous=%fs new=%fs)",
					1.0/(stamp.seconds()-previousStamp_), expectedUpdateRate_, previousStamp_, stamp.seconds());
			return;
		}

		if(!groundTruthFrameId_.empty())
		{
			groundTruth = getTransform(groundTruthFrameId_, groundTruthBaseFrameId_, stamp, *tfBuffer_, waitForTransform_);

			if(!data.imageRaw().empty() || !data.laserScanRaw().isEmpty())
			{
				if(odometry_->getPose().isIdentity())
				{
					// sync with the first value of the ground truth
					if(groundTruth.isNull())
					{
						RCLCPP_WARN(this->get_logger(), "Ground truth frames \"%s\" -> \"%s\" are set but failed to "
								"get them, odometry won't be initialized with ground truth.",
								groundTruthFrameId_.c_str(), groundTruthBaseFrameId_.c_str());
					}
					else
					{
						RCLCPP_INFO(this->get_logger(), "Initializing odometry pose to %s (from \"%s\" -> \"%s\")",
								groundTruth.prettyPrint().c_str(),
								groundTruthFrameId_.c_str(),
								groundTruthBaseFrameId_.c_str());
						odometry_->reset(groundTruth);
					}
				}
			}
		}
	}


	Transform guessCurrentPose;
	if(!guessFrameId_.empty())
	{
		guessCurrentPose = getTransform(guessFrameId_, frameId_, stamp, *tfBuffer_, waitForTransform_);
		Transform previousPose = guessPreviousPose_.isNull()?guessCurrentPose:guessPreviousPose_;
		if(!previousPose.isNull() && !guessCurrentPose.isNull())
		{
			if(guess_.isNull())
			{
				guess_ = previousPose.inverse() * guessCurrentPose;
			}
			else
			{
				guess_ = guess_ * previousPose.inverse() * guessCurrentPose;
			}
			if(!guessPreviousPose_.isNull() && (guessMinTranslation_ > 0.0 || guessMinRotation_ > 0.0))
			{
				float x,y,z,roll,pitch,yaw;
				guess_.getTranslationAndEulerAngles(x,y,z,roll,pitch,yaw);
				if((guessMinTranslation_ <= 0.0 || uMax3(fabs(x), fabs(y), fabs(z)) < guessMinTranslation_) &&
				   (guessMinRotation_ <= 0.0 || uMax3(fabs(roll), fabs(pitch), fabs(yaw)) < guessMinRotation_) &&
				   (guessMinTime_ <= 0.0 || (previousStamp_>0.0 && stamp.seconds()-previousStamp_ < guessMinTime_)))
				{
					// Ignore odometry update, we didn't move enough
					if(publishTf_)
					{
						geometry_msgs::msg::TransformStamped correctionMsg;
						correctionMsg.child_frame_id = guessFrameId_;
						correctionMsg.header.frame_id = odomFrameId_;
						correctionMsg.header.stamp = stamp;
						Transform correction = odometry_->getPose() * guess_ * guessCurrentPose.inverse();
						rtabmap_ros::transformToGeometryMsg(correction, correctionMsg.transform);
						tfBroadcaster_->sendTransform(correctionMsg);
					}
					guessPreviousPose_ = guessCurrentPose;
					return;
				}
			}
			guessPreviousPose_ = guessCurrentPose;
		}
		else
		{
			RCLCPP_ERROR(this->get_logger(), "\"guess_from_tf\" is true, but guess cannot be computed between frames \"%s\" -> \"%s\". Aborting odometry update...", guessFrameId_.c_str(), frameId_.c_str());
			return;
		}
	}

	// process data
	rclcpp::Time timeStart = now();
	rtabmap::OdometryInfo info;
	SensorData dataCpy = data;
	if(!groundTruth.isNull())
	{
		dataCpy.setGroundTruth(groundTruth);
	}
	rtabmap::Transform pose = odometry_->process(dataCpy, guess_, &info);
	guess_.setNull();
	if(!pose.isNull())
	{
		resetCurrentCount_ = resetCountdown_;

		//*********************
		// Update odometry
		//*********************
		geometry_msgs::msg::TransformStamped poseMsg;
		poseMsg.child_frame_id = frameId_;
		poseMsg.header.frame_id = odomFrameId_;
		poseMsg.header.stamp = stamp;
		rtabmap_ros::transformToGeometryMsg(pose, poseMsg.transform);

		if(publishTf_)
		{
			if(!guessFrameId_.empty())
			{
				//publish correction of actual odometry so we have /odom -> /odom_guess -> /base_link
				geometry_msgs::msg::TransformStamped correctionMsg;
				correctionMsg.child_frame_id = guessFrameId_;
				correctionMsg.header.frame_id = odomFrameId_;
				correctionMsg.header.stamp = stamp;
				Transform correction = pose * guessCurrentPose.inverse();
				rtabmap_ros::transformToGeometryMsg(correction, correctionMsg.transform);
				tfBroadcaster_->sendTransform(correctionMsg);
			}
			else
			{
				tfBroadcaster_->sendTransform(poseMsg);
			}
		}

		if(odomPub_->get_subscription_count())
		{
			//next, we'll publish the odometry message over ROS
			nav_msgs::msg::Odometry odom;
			odom.header.stamp = stamp; // use corresponding time stamp to image
			odom.header.frame_id = odomFrameId_;
			odom.child_frame_id = frameId_;

			//set the position
			odom.pose.pose.position.x = poseMsg.transform.translation.x;
			odom.pose.pose.position.y = poseMsg.transform.translation.y;
			odom.pose.pose.position.z = poseMsg.transform.translation.z;
			odom.pose.pose.orientation = poseMsg.transform.rotation;

			//set covariance
			// libviso2 uses approximately vel variance * 2
			odom.pose.covariance.at(0) = info.reg.covariance.at<double>(0,0)*2;  // xx
			odom.pose.covariance.at(7) = info.reg.covariance.at<double>(1,1)*2;  // yy
			odom.pose.covariance.at(14) = info.reg.covariance.at<double>(2,2)*2; // zz
			odom.pose.covariance.at(21) = info.reg.covariance.at<double>(3,3)*2; // rr
			odom.pose.covariance.at(28) = info.reg.covariance.at<double>(4,4)*2; // pp
			odom.pose.covariance.at(35) = info.reg.covariance.at<double>(5,5)*2; // yawyaw

			//set velocity
			bool setTwist = !odometry_->getVelocityGuess().isNull();
			if(setTwist)
			{
				float x,y,z,roll,pitch,yaw;
				odometry_->getVelocityGuess().getTranslationAndEulerAngles(x,y,z,roll,pitch,yaw);
				odom.twist.twist.linear.x = x;
				odom.twist.twist.linear.y = y;
				odom.twist.twist.linear.z = z;
				odom.twist.twist.angular.x = roll;
				odom.twist.twist.angular.y = pitch;
				odom.twist.twist.angular.z = yaw;
			}

			odom.twist.covariance.at(0) = setTwist?info.reg.covariance.at<double>(0,0):BAD_COVARIANCE;  // xx
			odom.twist.covariance.at(7) = setTwist?info.reg.covariance.at<double>(1,1):BAD_COVARIANCE;  // yy
			odom.twist.covariance.at(14) = setTwist?info.reg.covariance.at<double>(2,2):BAD_COVARIANCE; // zz
			odom.twist.covariance.at(21) = setTwist?info.reg.covariance.at<double>(3,3):BAD_COVARIANCE; // rr
			odom.twist.covariance.at(28) = setTwist?info.reg.covariance.at<double>(4,4):BAD_COVARIANCE; // pp
			odom.twist.covariance.at(35) = setTwist?info.reg.covariance.at<double>(5,5):BAD_COVARIANCE; // yawyaw

			//publish the message
			if(setTwist || publishNullWhenLost_)
			{
				odomPub_->publish(odom);
			}
		}

		// local map / reference frame
		if(odomLocalMap_->get_subscription_count() && !info.localMap.empty())
		{
			pcl::PointCloud<pcl::PointXYZRGB> cloud;
			for(std::map<int, cv::Point3f>::const_iterator iter=info.localMap.begin(); iter!=info.localMap.end(); ++iter)
			{
				bool inlier = info.words.find(iter->first) != info.words.end();
				pcl::PointXYZRGB pt;
				pt.r = inlier?0:255;
				pt.g = 255;
				pt.x = iter->second.x;
				pt.y = iter->second.y;
				pt.z = iter->second.z;
				cloud.push_back(pt);
			}
			sensor_msgs::msg::PointCloud2 cloudMsg;
			pcl::toROSMsg(cloud, cloudMsg);
			cloudMsg.header.stamp = stamp; // use corresponding time stamp to image
			cloudMsg.header.frame_id = odomFrameId_;
			odomLocalMap_->publish(cloudMsg);
		}

		if(odomLastFrame_->get_subscription_count())
		{
			// check which type of Odometry is using
			if(odometry_->getType() == Odometry::kTypeF2M) // If it's Frame to Map Odometry
			{
				const std::vector<cv::Point3f> & words3  = ((OdometryF2M*)odometry_)->getLastFrame().getWords3();
				if(words3.size())
				{
					pcl::PointCloud<pcl::PointXYZ> cloud;
					for(std::vector<cv::Point3f>::const_iterator iter=words3.begin(); iter!=words3.end(); ++iter)
					{
						// transform to odom frame
						cv::Point3f pt = util3d::transformPoint(*iter, pose);
						cloud.push_back(pcl::PointXYZ(pt.x, pt.y, pt.z));
					}

					sensor_msgs::msg::PointCloud2 cloudMsg;
					pcl::toROSMsg(cloud, cloudMsg);
					cloudMsg.header.stamp = stamp; // use corresponding time stamp to image
					cloudMsg.header.frame_id = odomFrameId_;
					odomLastFrame_->publish(cloudMsg);
				}
			}
			else if(odometry_->getType() == Odometry::kTypeF2F) // if Using Frame to Frame Odometry
			{
				const Signature & refFrame = ((OdometryF2F*)odometry_)->getRefFrame();

				if(refFrame.getWords3().size())
				{
					pcl::PointCloud<pcl::PointXYZ> cloud;
					for(std::vector<cv::Point3f>::const_iterator iter=refFrame.getWords3().begin(); iter!=refFrame.getWords3().end(); ++iter)
					{
						// transform to odom frame
						cv::Point3f pt = util3d::transformPoint(*iter, pose);
						cloud.push_back(pcl::PointXYZ(pt.x, pt.y, pt.z));
					}
					sensor_msgs::msg::PointCloud2 cloudMsg;
					pcl::toROSMsg(cloud, cloudMsg);
					cloudMsg.header.stamp = stamp; // use corresponding time stamp to image
					cloudMsg.header.frame_id = odomFrameId_;
					odomLastFrame_->publish(cloudMsg);
				}
			}
		}

		if(odomLocalScanMap_->get_subscription_count() && !info.localScanMap.isEmpty())
		{
			sensor_msgs::msg::PointCloud2 cloudMsg;
			if(info.localScanMap.hasNormals())
			{
				pcl::PointCloud<pcl::PointNormal>::Ptr cloud = util3d::laserScanToPointCloudNormal(info.localScanMap, info.localScanMap.localTransform());
				pcl::toROSMsg(*cloud, cloudMsg);
			}
			else
			{
				pcl::PointCloud<pcl::PointXYZ>::Ptr cloud = util3d::laserScanToPointCloud(info.localScanMap, info.localScanMap.localTransform());
				pcl::toROSMsg(*cloud, cloudMsg);
			}

			cloudMsg.header.stamp = stamp; // use corresponding time stamp to image
			cloudMsg.header.frame_id = odomFrameId_;
			odomLocalScanMap_->publish(cloudMsg);
		}
	}
	else if(data.imageRaw().empty() && data.laserScanRaw().isEmpty() && !data.imu().empty())
	{
		return;
	}
	else if(publishNullWhenLost_)
	{
		//RCLCPP_WARN(this->get_logger(), "Odometry lost!");

		//send null pose to notify that odometry is lost
		nav_msgs::msg::Odometry odom;
		odom.header.stamp = stamp; // use corresponding time stamp to image
		odom.header.frame_id = odomFrameId_;
		odom.child_frame_id = frameId_;
		odom.pose.covariance.at(0) = BAD_COVARIANCE;  // xx
		odom.pose.covariance.at(7) = BAD_COVARIANCE;  // yy
		odom.pose.covariance.at(14) = BAD_COVARIANCE; // zz
		odom.pose.covariance.at(21) = BAD_COVARIANCE; // rr
		odom.pose.covariance.at(28) = BAD_COVARIANCE; // pp
		odom.pose.covariance.at(35) = BAD_COVARIANCE; // yawyaw
		odom.twist.covariance.at(0) = BAD_COVARIANCE;  // xx
		odom.twist.covariance.at(7) = BAD_COVARIANCE;  // yy
		odom.twist.covariance.at(14) = BAD_COVARIANCE; // zz
		odom.twist.covariance.at(21) = BAD_COVARIANCE; // rr
		odom.twist.covariance.at(28) = BAD_COVARIANCE; // pp
		odom.twist.covariance.at(35) = BAD_COVARIANCE; // yawyaw

		//publish the message
		odomPub_->publish(odom);
	}

	if(pose.isNull() && resetCurrentCount_ > 0)
	{
		RCLCPP_WARN(this->get_logger(), "Odometry lost! Odometry will be reset after next %d consecutive unsuccessful odometry updates...", resetCurrentCount_);

		--resetCurrentCount_;
		if(resetCurrentCount_ == 0)
		{
			// Check TF to see if sensor fusion is used (e.g., the output of robot_localization)
			Transform tfPose = getTransform(odomFrameId_, frameId_, stamp, *tfBuffer_, waitForTransform_);
			if(tfPose.isNull())
			{
				RCLCPP_WARN(this->get_logger(), "Odometry automatically reset to latest computed pose!");
				odometry_->reset(odometry_->getPose());
			}
			else
			{
				RCLCPP_WARN(this->get_logger(), "Odometry automatically reset to latest odometry pose available from TF (%s->%s)!",
						odomFrameId_.c_str(), frameId_.c_str());
				odometry_->reset(tfPose);
			}

		}
	}

	if(odomInfoPub_->get_subscription_count())
	{
		rtabmap_ros::msg::OdomInfo infoMsg;
		odomInfoToROS(info, infoMsg);
		infoMsg.header.stamp = stamp; // use corresponding time stamp to image
		infoMsg.header.frame_id = odomFrameId_;
		odomInfoPub_->publish(infoMsg);
	}

	if(!data.imageRaw().empty() || !data.laserScanRaw().isEmpty())
	{
		if(visParams_)
		{
			if(icpParams_)
			{
				RCLCPP_INFO(this->get_logger(), "Odom: quality=%d, ratio=%f, std dev=%fm|%frad, update time=%fs", info.reg.inliers, info.reg.icpInliersRatio, pose.isNull()?0.0f:std::sqrt(info.reg.covariance.at<double>(0,0)), pose.isNull()?0.0f:std::sqrt(info.reg.covariance.at<double>(5,5)), (now()-timeStart).seconds());
			}
			else
			{
				RCLCPP_INFO(this->get_logger(), "Odom: quality=%d, std dev=%fm|%frad, update time=%fs", info.reg.inliers, pose.isNull()?0.0f:std::sqrt(info.reg.covariance.at<double>(0,0)), pose.isNull()?0.0f:std::sqrt(info.reg.covariance.at<double>(5,5)), (now()-timeStart).seconds());
			}
		}
		else // if(icpParams_)
		{
			RCLCPP_INFO(this->get_logger(), "Odom: ratio=%f, std dev=%fm|%frad, update time=%fs", info.reg.icpInliersRatio, pose.isNull()?0.0f:std::sqrt(info.reg.covariance.at<double>(0,0)), pose.isNull()?0.0f:std::sqrt(info.reg.covariance.at<double>(5,5)), (now()-timeStart).seconds());
		}
		previousStamp_ = stamp.seconds();
	}
}

void OdometryROS::resetOdom(
		const std::shared_ptr<rmw_request_id_t>,
		const std::shared_ptr<std_srvs::srv::Empty::Request>,
		std::shared_ptr<std_srvs::srv::Empty::Response>)
{
	RCLCPP_INFO(this->get_logger(), "visual_odometry: reset odom!");
	reset();
}

void OdometryROS::resetToPose(
		const std::shared_ptr<rmw_request_id_t>,
		const std::shared_ptr<rtabmap_ros::srv::ResetPose::Request> req,
		std::shared_ptr<rtabmap_ros::srv::ResetPose::Response>)
{
	Transform pose(req->x, req->y, req->z, req->roll, req->pitch, req->yaw);
	RCLCPP_INFO(this->get_logger(), "visual_odometry: reset odom to pose %s!", pose.prettyPrint().c_str());
	reset(pose);
}

void OdometryROS::reset(const Transform & pose)
{
	odometry_->reset(pose);
	guess_.setNull();
	guessPreviousPose_.setNull();
	previousStamp_ = 0.0;
	resetCurrentCount_ = resetCountdown_;
	imuProcessed_ = false;
	bufferedData_= SensorData();
	lastImuReceivedStamp_=0.0;
	this->flushCallbacks();
}

void OdometryROS::pause(
		const std::shared_ptr<rmw_request_id_t>,
		const std::shared_ptr<std_srvs::srv::Empty::Request>,
		std::shared_ptr<std_srvs::srv::Empty::Response>)
{
	if(paused_)
	{
		RCLCPP_WARN(this->get_logger(), "Odometry: Already paused!");
	}
	else
	{
		paused_ = true;
		RCLCPP_INFO(this->get_logger(), "Odometry: paused!");
	}
}

void OdometryROS::resume(
		const std::shared_ptr<rmw_request_id_t>,
		const std::shared_ptr<std_srvs::srv::Empty::Request>,
		std::shared_ptr<std_srvs::srv::Empty::Response>)
{
	if(!paused_)
	{
		RCLCPP_WARN(this->get_logger(), "Odometry: Already running!");
	}
	else
	{
		paused_ = false;
		RCLCPP_INFO(this->get_logger(), "Odometry: resumed!");
	}
}

void OdometryROS::setLogDebug(
		const std::shared_ptr<rmw_request_id_t>,
		const std::shared_ptr<std_srvs::srv::Empty::Request>,
		std::shared_ptr<std_srvs::srv::Empty::Response>)
{
	RCLCPP_INFO(this->get_logger(), "visual_odometry: Set log level to Debug");
	ULogger::setLevel(ULogger::kDebug);
}
void OdometryROS::setLogInfo(
		const std::shared_ptr<rmw_request_id_t>,
		const std::shared_ptr<std_srvs::srv::Empty::Request>,
		std::shared_ptr<std_srvs::srv::Empty::Response>)
{
	RCLCPP_INFO(this->get_logger(), "visual_odometry: Set log level to Info");
	ULogger::setLevel(ULogger::kInfo);
}
void OdometryROS::setLogWarn(
		const std::shared_ptr<rmw_request_id_t>,
		const std::shared_ptr<std_srvs::srv::Empty::Request>,
		std::shared_ptr<std_srvs::srv::Empty::Response>)
{
	RCLCPP_INFO(this->get_logger(), "visual_odometry: Set log level to Warning");
	ULogger::setLevel(ULogger::kWarning);
}
void OdometryROS::setLogError(
		const std::shared_ptr<rmw_request_id_t>,
		const std::shared_ptr<std_srvs::srv::Empty::Request>,
		std::shared_ptr<std_srvs::srv::Empty::Response>)
{
	RCLCPP_INFO(this->get_logger(), "visual_odometry: Set log level to Error");
	ULogger::setLevel(ULogger::kError);
}


}

#include "rclcpp_components/register_node_macro.hpp"

// Register the component with class_loader.
// This acts as a sort of entry point, allowing the component to be discoverable when its library
// is being loaded into a running process.
RCLCPP_COMPONENTS_REGISTER_NODE(rtabmap_ros::OdometryROS)
