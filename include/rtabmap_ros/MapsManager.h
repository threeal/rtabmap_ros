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

#ifndef MAPSMANAGER_H_
#define MAPSMANAGER_H_

#include <rtabmap/core/Signature.h>
#include <rtabmap/core/Parameters.h>
#include <rtabmap/core/FlannIndex.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>

namespace rtabmap {
class OctoMap;
class Memory;
class OccupancyGrid;

}  // namespace rtabmap

class MapsManager {
public:
	MapsManager();
	virtual ~MapsManager();
	void init(rclcpp::Node & node, const std::string & name, bool usePublicNamespace);
	void clear();
	bool hasSubscribers() const;
	void backwardCompatibilityParameters(rclcpp::Node & node, rtabmap::ParametersMap & parameters) const;
	void setParameters(const rtabmap::ParametersMap & parameters);
	void set2DMap(const cv::Mat & map, float xMin, float yMin, float cellSize, const std::map<int, rtabmap::Transform> & poses, const rtabmap::Memory * memory = 0);

	std::map<int, rtabmap::Transform> getFilteredPoses(
			const std::map<int, rtabmap::Transform> & poses);

	std::map<int, rtabmap::Transform> updateMapCaches(
			const std::map<int, rtabmap::Transform> & poses,
			const rtabmap::Memory * memory,
			bool updateGrid,
			bool updateOctomap,
			const std::map<int, rtabmap::Signature> & signatures = std::map<int, rtabmap::Signature>());

	void publishMaps(
			const std::map<int, rtabmap::Transform> & poses,
			const rclcpp::Time & stamp,
			const std::string & mapFrameId);

	cv::Mat getGridMap(
			float & xMin,
			float & yMin,
			float & gridCellSize);

	cv::Mat getGridProbMap(
			float & xMin,
			float & yMin,
			float & gridCellSize);

	const rtabmap::OctoMap * getOctomap() const {return octomap_;}
	const rtabmap::OccupancyGrid * getOccupancyGrid() const {return occupancyGrid_;}

private:
	// mapping stuff
	bool cloudOutputVoxelized_;
	bool cloudSubtractFiltering_;
	int cloudSubtractFilteringMinNeighbors_;
	double mapFilterRadius_;
	double mapFilterAngle_;
	bool mapCacheCleanup_;
	bool alwaysUpdateMap_;
	bool scanEmptyRayTracing_;

	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr cloudMapPub_;
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr cloudGroundPub_;
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr cloudObstaclesPub_;
	rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr gridMapPub_;
	rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr gridProbMapPub_;
#ifdef WITH_OCTOMAP_MSGS
#ifdef RTABMAP_OCTOMAP
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr octoMapPubBin_;
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr octoMapPubFull_;
#endif
#endif
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr octoMapCloud_;
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr octoMapFrontierCloud_;
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr octoMapGroundCloud_;
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr octoMapObstacleCloud_;
	rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr octoMapEmptySpace_;
	rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr octoMapProj_;

	std::map<int, rtabmap::Transform> assembledGroundPoses_;
	std::map<int, rtabmap::Transform> assembledObstaclePoses_;
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr assembledObstacles_;
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr assembledGround_;
	rtabmap::FlannIndex assembledGroundIndex_;
	rtabmap::FlannIndex assembledObstacleIndex_;
	std::map<int, pcl::PointCloud<pcl::PointXYZRGB>::Ptr > groundClouds_;
	std::map<int, pcl::PointCloud<pcl::PointXYZRGB>::Ptr > obstacleClouds_;

	std::map<int, rtabmap::Transform> gridPoses_;
	cv::Mat gridMap_;
	std::map<int, std::pair< std::pair<cv::Mat, cv::Mat>, cv::Mat> > gridMaps_; // < <ground, obstacles>, empty cells >
	std::map<int, cv::Point3f> gridMapsViewpoints_;

	rtabmap::OccupancyGrid * occupancyGrid_;
	bool gridUpdated_;

	rtabmap::OctoMap * octomap_;
	int octomapTreeDepth_;
	bool octomapUpdated_;

	rtabmap::ParametersMap parameters_;

	bool latching_;
	std::map<void*, bool> latched_;
};

#endif /* MAPSMANAGER_H_ */
