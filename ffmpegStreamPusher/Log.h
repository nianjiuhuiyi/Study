#ifndef Log_H
#define LOG_H

//  __FILE__ ��ȡԴ�ļ������·��������
//  __LINE__ ��ȡ���д������ļ��е��к�
//  __func__ �� __FUNCTION__ ��ȡ������

#define LOGI(format, ...) fprintf(stderr, "[INFO]%s [%s:%d %s()] " format "\n", "_",__FILE__,__LINE__,__func__ ,##__VA_ARGS__)
#define LOGE(format, ...)  fprintf(stderr,"[ERROR]%s [%s:%d %s()] " format "\n","_",__FILE__,__LINE__,__func__ ,##__VA_ARGS__)

#endif // !Log_H

