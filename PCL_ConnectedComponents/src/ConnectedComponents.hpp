#include <iostream>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/search/search.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/segmentation/organized_connected_component_segmentation.h>
#include <pcl/segmentation/euclidean_cluster_comparator.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/openni_grabber.h>
#include <pcl/io/pcd_grabber.h>
#include <pcl/features/normal_3d.h>
#include <pcl/common/transforms.h>
#include <pcl/features/integral_image_normal.h>
#include <pcl/segmentation/organized_multi_plane_segmentation.h>
#include <pcl/segmentation/euclidean_cluster_comparator.h>
#include <pcl/segmentation/organized_connected_component_segmentation.h>
#include <pcl/common/time.h>

template <typename PointT>
class ConnectedComponents {
public:
    typedef pcl::PointCloud<PointT> Cloud;
    typedef typename Cloud::ConstPtr CloudConstPtr;
    typedef typename Cloud::Ptr CloudPtr;
    CloudConstPtr cloud_;
    pcl::IntegralImageNormalEstimation<PointT, pcl::Normal> ne_;
    pcl::OrganizedMultiPlaneSegmentation<PointT, pcl::Normal, pcl::Label> mps_;

    ConnectedComponents(CloudConstPtr cloud) : cloud_(cloud)
    {
    }
    typename pcl::PointCloud<PointT>::CloudVectorType SegmentCloud();
};

template<typename PointT>
typename pcl::PointCloud<PointT>::CloudVectorType ConnectedComponents<PointT>::SegmentCloud() {
    CloudPtr cloud (new Cloud ());
    {
        *cloud = *cloud_;
    }

    unsigned char red [6] = {255,   0,   0, 255, 255,   0};
    unsigned char grn [6] = {  0, 255,   0, 255,   0, 255};
    unsigned char blu [6] = {  0,   0, 255,   0, 255, 255};
    pcl::PointCloud<pcl::PointXYZRGB> aggregate_cloud;

    pcl::PointCloud<pcl::Normal>::Ptr normal_cloud(new pcl::PointCloud<pcl::Normal>);
    ne_.setInputCloud(cloud);
    ne_.compute(*normal_cloud);

    std::vector<pcl::PlanarRegion<PointT>, Eigen::aligned_allocator<pcl::PlanarRegion<PointT> > > regions;
    std::vector<pcl::ModelCoefficients> model_coefficients;
    std::vector<pcl::PointIndices> inlier_indices;
    pcl::PointCloud<pcl::Label>::Ptr labels(new pcl::PointCloud<pcl::Label>);
    std::vector<pcl::PointIndices> label_indices;
    std::vector<pcl::PointIndices> boundary_indices;
    mps_.setInputNormals(normal_cloud);
    mps_.setInputCloud(cloud);
    mps_.segmentAndRefine(regions, model_coefficients, inlier_indices, labels, label_indices, boundary_indices);

    typename pcl::PointCloud<PointT>::CloudVectorType clusters;

    if (regions.size () > 0) {
        std::vector<bool> plane_labels;
        plane_labels.resize(label_indices.size(), false);
        for (size_t i = 0; i < label_indices.size(); i++) {
            if (label_indices[i].indices.size() > 10000) {
                plane_labels[i] = true;
            }
        }
        typename pcl::EuclideanClusterComparator<PointT, pcl::Label>::Ptr euclidean_cluster_comparator_(
                new typename pcl::EuclideanClusterComparator<PointT, pcl::Label>());
        euclidean_cluster_comparator_->setInputCloud(cloud);
        euclidean_cluster_comparator_->setLabels(labels);
        // euclidean_cluster_comparator_->setExcludeLabels(plane_labels);
        euclidean_cluster_comparator_->setDistanceThreshold(0.01f, false);

        pcl::PointCloud<pcl::Label> euclidean_labels;
        std::vector<pcl::PointIndices> euclidean_label_indices;
        pcl::OrganizedConnectedComponentSegmentation<PointT, pcl::Label> euclidean_segmentation(
                euclidean_cluster_comparator_);
        euclidean_segmentation.setInputCloud(cloud);
        euclidean_segmentation.segment(euclidean_labels, euclidean_label_indices);

        for (size_t i = 0; i < euclidean_label_indices.size(); i++) {
            if (euclidean_label_indices[i].indices.size() > 1000) {
                pcl::PointCloud<PointT> cluster;
                pcl::copyPointCloud(*cloud, euclidean_label_indices[i].indices, cluster);
                clusters.push_back(cluster);
                pcl::PointCloud<pcl::PointXYZRGB> color_cluster;
                pcl::copyPointCloud(cluster, color_cluster);
                for (size_t j = 0; j < color_cluster.size(); j++) {
                    color_cluster.points[j].r = (color_cluster.points[j].r + red[i % 6]) / 2;
                    color_cluster.points[j].g = (color_cluster.points[j].g + grn[i % 6]) / 2;
                    color_cluster.points[j].b = (color_cluster.points[j].b + blu[i % 6]) / 2;
                }
                aggregate_cloud += color_cluster;

            }
        }
    }

    return clusters;
}