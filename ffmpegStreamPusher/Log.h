#ifndef Log_H
#define LOG_H

//  __FILE__ 获取源文件的相对路径和名字
//  __LINE__ 获取该行代码在文件中的行号
//  __func__ 或 __FUNCTION__ 获取函数名

#define LOGI(format, ...) fprintf(stderr, "[INFO]%s [%s:%d %s()] " format "\n", "_",__FILE__,__LINE__,__func__ ,##__VA_ARGS__)
#define LOGE(format, ...)  fprintf(stderr,"[ERROR]%s [%s:%d %s()] " format "\n","_",__FILE__,__LINE__,__func__ ,##__VA_ARGS__)

#endif // !Log_H

