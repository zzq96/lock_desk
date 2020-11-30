#include"flask.h"
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string send_char_arr(char * memblock, long size){
    CURL *curl;
    CURLcode res;

    string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://49.52.10.229:5000/predict");

        // disable Expect:
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Content-Type: opencv/image"); // use strange type to prevent server to parse content
        chunk = curl_slist_append(chunk, "Expect:");
        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        // data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, memblock);
        // data length
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, size);

        // response handler
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        /* Perform the request, res will get the return code */ 
        res = curl_easy_perform(curl);

        /* Check for errors */ 
        if(res != CURLE_OK){
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
        }

        /* always cleanup */ 
        curl_easy_cleanup(curl);
    }

    //delete[] memblock; // memblock should be freed from called here.
    return readBuffer;
}

string send_mat(cv::Mat mat){
    vector<uchar> buf;
    imencode(".png", mat, buf);
    char * memblock = reinterpret_cast<char*>(buf.data());
    long size = buf.size();

    return send_char_arr(memblock, size);
}

void getMasks(cv::Mat& mat, vector<cv::Mat> & masks, vector<int> &Vclasses)
{
    masks.clear();
	string resp = send_mat(mat);
	Document doc;
	doc.Parse(resp.c_str());
	bool success = doc["success"].GetBool();
	if (doc.HasMember("success") && doc["success"].IsBool())
	{
		cout << "get mask "<<success << endl;
	}
	//5.1 ������������
    vector<vector<cv::Point2d>> points;
	int id, row, col;
    points.clear();
	if(doc.HasMember("instances_id") && doc["instances_id"].IsArray()\
		&&doc.HasMember("instances_row") && doc["instances_row"].IsArray()
		&&doc.HasMember("instances_col") && doc["instances_col"].IsArray())
	{
		//5.1.1 ���ֶ�ת����Ϊrapidjson::Value����
		const rapidjson::Value& ids = doc["instances_id"];
		const rapidjson::Value& rows = doc["instances_row"];
		const rapidjson::Value& cols = doc["instances_col"];
		//5.1.2 ��ȡ���鳤��
		size_t len = ids.Size();
		//5.1.3 �����±������ע�⽫Ԫ��ת��Ϊ��Ӧ���ͣ�����Ҫ����GetInt()
		for(size_t i = 0; i < len; i++)
		{
			id = ids[i].GetInt();
			row = rows[i].GetInt();
			col = cols[i].GetInt();
			if (id + 1 > points.size())
				points.push_back(vector<cv::Point2d>());
			points[id].push_back(cv::Point2d(col, row));
		}
	}
    int object_num = points.size();
    Vclasses.clear();
	if(doc.HasMember("pred_classes") && doc["pred_classes"].IsArray())
		{
			//5.1.1 ���ֶ�ת����Ϊrapidjson::Value����
			const rapidjson::Value& classes = doc["pred_classes"];
			//5.1.2 ��ȡ���鳤��
			size_t len = classes.Size();
			assert(len == object_num);
			//5.1.3 �����±������ע�⽫Ԫ��ת��Ϊ��Ӧ���ͣ�����Ҫ����GetInt()
			for(size_t i = 0; i < len; i++)
			{
                Vclasses.push_back(classes[i].GetInt());
			}
		}
	for (int i = 0; i < points.size(); i++)
	 {
        masks.push_back(cv::Mat::zeros(mat.rows, mat.cols, CV_8UC1));
        for (int j = 0; j < points[i].size(); j++)
        {
            
            masks[i].at<UINT8>(points[i][j].y, points[i][j].x) = 255;
        }
    }
	//cout << "issuccess: " << issuccess  << endl;
	//GenericArray  arr = d["instances"].GetArray();
}
