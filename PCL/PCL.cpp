#include <pcl/visualization/cloud_viewer.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <iostream>
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>   // .ply点云格式
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);
//VTK_MODULE_INIT(vtkRenderingOpenGL);  # 教程是里这行，是错的，新的是下面这个库了，不该就会产生“无法解析的外部符号的错误”，就是因为库没被找到
VTK_MODULE_INIT(vtkRenderingOpenGL2);

int user_data;

void viewerOneOff(pcl::visualization::PCLVisualizer& viewer) {
	viewer.setBackgroundColor(1.0, 0.5, 1.0);
	pcl::PointXYZ o;
	o.x = 1.0;
	o.y = 0;
	o.z = 0;
	viewer.addSphere(o, 0.25, "sphere", 0);
	std::cout << "i only run once" << std::endl;
}

void viewerPsycho(pcl::visualization::PCLVisualizer& viewer) {
	static unsigned count = 0;
	std::stringstream ss;
	ss << "Once per viewer loop: " << count++;
	viewer.removeShape("text", 0);
	viewer.addText(ss.str(), 200, 300, "text", 0);

	//FIXME: possible race condition here:
	user_data++;
}

/*
	运行报错时一定注意看是不是Debug模式，库用的debug的，用release的会报错。
*/

int main() {
	//pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGBA>);
	//pcl::io::loadPCDFile(R"(C:\Users\Administrator\Downloads\rabbit.pcd)", *cloud);

	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGBA>());
	//pcl::io::loadPLYFile<pcl::PointXYZRGBA>(R"(F:\colmap\peach\0525\fused_ACSII.ply)", *cloud);
	pcl::io::loadPLYFile<pcl::PointXYZRGBA>(R"(F:\colmap\peach\0525\fused.ply)", *cloud);   // 内容一定要是二进制的才行


	//pcl::PolygonMesh mesh;
	//pcl::io::loadPLYFile(R"(F:\colmap\peach\0525\meshed-poisson.ply)", mesh);





	pcl::visualization::CloudViewer viewer("Cloud Viewer");

	//blocks until the cloud is actually rendered
	viewer.showCloud(cloud);

	//use the following functions to get access to the underlying more advanced/powerful
	//PCLVisualizer

	//This will only get called once
	viewer.runOnVisualizationThreadOnce(viewerOneOff);

	//This will get called once per visualization iteration
	viewer.runOnVisualizationThread(viewerPsycho);
	while (!viewer.wasStopped()) {
		//you can also do cool processing here
		//FIXME: Note that this is running in a separate thread from viewerPsycho
		//and you should guard against race conditions yourself...
		user_data++;
	}
	return 0;
}