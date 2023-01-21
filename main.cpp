#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <codecvt>
#include <cstring>
#include <locale>
#include <getopt.h>
#include <unistd.h>
#include <curl/curl.h>
#include "cJSON/cJSON.h"
using namespace std;

#define CTRL_RED "\033[31m"
#define CTRL_GEEEN "\033[32m"
#define CTRL_RESET "\033[0m"
#define CTRL_BLOOD "\033[1m"
#define CTRL_CLEAR "\033[2J"
#define CTRL_CURINIT "\033[1;1H"

const char HELP_MSG[]= 
    "usage:\n"
    "    select-translate [options]\n"
    "options:\n"
    "    -c             --clear             clean screen before translation\n"
    "    -d <time/s>    --delay             the time between two translation (default 3)\n"
    "    -s <lang>      --source-language   source language. (default auto)\n"
    "                                       [Chinese/English/Japanese/Chinese_Simplified\n"
    "                                        /Chinese_traditional/auto]\n"
    "    -t <lang>      --target-language   target language. (default Chinese)\n"
    "                                       [Chinese/English/Japanese/Chinese_Simplified\n"
    "                                        /Chinese_traditional]\n"
    "    -h             --help              Display available options.\n";

class Exception : public exception
{
public:
    string msg;

    Exception(): msg("unknow error") {};

    Exception(string msg): msg(msg) {};

    const char * what() const throw () {
        return msg.c_str();
    }
};

class CodeConvert {
public:
    static uint8_t ToHex(unsigned char x) 
    { 
        return  x > 9 ? x + 55 : x + 48; 
    }

    static string urlEncode(const string &str) {
        string strTemp = "";
        size_t length = str.length();
        for (size_t i = 0; i < length; i++)
        {
            if (isalnum((unsigned char)str[i]) || 
                (str[i] == '-') ||
                (str[i] == '_') || 
                (str[i] == '.') || 
                (str[i] == '~'))
                strTemp += str[i];
            else if (str[i] == ' ')
                strTemp += "+";
            else
            {
                strTemp += '%';
                strTemp += ToHex((unsigned char)str[i] >> 4);
                strTemp += ToHex((unsigned char)str[i] % 16);
            }
        }
        return strTemp;
    }
};

class Translate{
public:
    struct Options{
        // clear screean
        bool clear;
        // sleep time(s)
        int delay;
        // source  language
        string sl;
        // target language
        string tl;
    };

    static const int MAX_BUFFER_SIZE = 5000;
    static char format_buffer[MAX_BUFFER_SIZE];
    static string select_buffer;
    static Options options;

    static void getOption(int argc, char** argv) {
        static int opt = 0, lopt = 0, loidx = 0;

        static struct option long_options[] = {
            {"clear-screeen", no_argument, NULL, 'c'},
            {"delay", required_argument, NULL, 'd'},
            {"source-language", required_argument, NULL, 's'},
            {"target-language", required_argument, NULL, 't'},
            {"help", no_argument, NULL, 'h'},
            {0, 0, 0, 0}
        };

        map<string,string> lang_map = {
            {"Chinese", "zh-CN"},
            {"English", "en"},
            {"Japanese", "ja"},
            {"Chinese_Simplified", "zh-CN"},
            {"Chinese_Traditional", "zh-TW"},
            {"auto", "auto"}
        };

        while ((opt = getopt_long(argc, argv, "cd:s:l:h", long_options, &loidx)) != -1) {
            if (opt == 0) {
                opt = lopt;
            }
            string arg_string;
            int arg_int;
            switch (opt) {
                case 'c':
                    options.clear = true;
                    break;
                case 'd':
                    arg_string = string(optarg);
                    arg_int = stoi(arg_string);
                    options.delay = arg_int < 0 ? 3 : arg_int;
                    break;
                case 's':
                    arg_string = string(optarg);
                    if(lang_map.find(arg_string) == lang_map.end()) options.sl = "auto";
                    else options.sl = lang_map[arg_string];
                    break;
                case 't':
                    arg_string = string(optarg);
                    if(lang_map.find(arg_string) == lang_map.end() || arg_string == "auto") options.tl = "chinese";
                    else options.tl = lang_map[arg_string];
                    break;
                case 'h':
                    cout << HELP_MSG << endl;
                    exit(0);
                    break;
                default:
                    break;
            }
        }
    }

    static void getSelect() {
        const char xsel[] = "xsel";
        char temp_buffer[101];
        int filled_size = 0;

        // reset select buffer
        Translate::select_buffer.clear();

        // get "xsel" output
        FILE* output_stream = nullptr;
        if((output_stream = popen(xsel, "r"))) {
            while (fgets(temp_buffer, 100, output_stream)) {
                filled_size += strlen(temp_buffer);
                Translate::select_buffer += temp_buffer;

                if(filled_size + 100 > MAX_BUFFER_SIZE) {
                    break;
                }
            }
            pclose(output_stream);
        } else {
            throw Exception("please install xsel");
        }
    }

    static int curlCallback(char* buffer, size_t size, size_t nmemb, void *userp) {
        string *str = dynamic_cast<string*>((string*)userp);
        str->append((char*)buffer, size * nmemb);
        return nmemb;
    };


    static int curlGet(string url, string *response) {
        CURL *curl = nullptr;
        CURLcode ret;
        struct curl_slist *headers = nullptr;


        curl = curl_easy_init();
        headers = curl_slist_append(headers, "Accept: Agent-007");

        if (curl && headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)response);
            ret = curl_easy_perform(curl);

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return ret;

        } else {
            return -1;
        }

        return 0;
    }

    static string googleTranslate(string src) {
    #ifndef DEBUG
        string url = "http://translate.google.com/translate_a/single?";
        string param = "client=gtx&sl=%s&tl=%s&dt=t&q=%s";

        string sl = options.sl;
        string tl = options.tl;

        src = CodeConvert::urlEncode(src);

        string format = url + param;
        sprintf(Translate::format_buffer, format.c_str(), 
            sl.c_str(),
            tl.c_str(),
            src.c_str()
        );

        string response;
        int sta = curlGet(Translate::format_buffer, &response);

        if(sta != -1) {
            string res;
            cJSON *json = cJSON_Parse(response.c_str());
            cJSON *arr1 = cJSON_GetArrayItem(json, 0);

            int size = cJSON_GetArraySize(arr1);
            for(int i = 0; i < size;i++) {
                cJSON *arr2 = cJSON_GetArrayItem(arr1, i);
                cJSON *str1 = cJSON_GetArrayItem(arr2, 0);

                if(str1 == NULL) {
                    cJSON_Delete(json);
                    throw Exception("failed to get transalte result");
                }

                res += cJSON_GetStringValue(str1);
            }

            cJSON_Delete(json);

            return res;
        } else {
            return "";
        }
    # else
        return src;
    # endif
    }

    static string filter(string src) {
        vector<string> from = {"\n", "%", "&", "#"};
        vector<string> to = {" ", "%25", "%26", "%23"};

        for(size_t i = 0; i < from.size(); i++) {
            string f = from[i], t = to[i];
            size_t idx;
            while((idx = src.find(f)) != string::npos) {
                src.replace(idx, f.size(), t);
            }
        }

        return src;
    }

    static void translateFromSelect(string &src, string &dist, int &count) {
        static string last_src;
        static string last_dist;
        static int cnt = 0;

        Translate::getSelect();
        src = Translate::select_buffer;

        if(src == last_src || src.empty()) {
            dist = last_dist;
            count = cnt;
            return;
        }

        last_src = src;
        src = filter(src);
        cnt++;
        count = cnt;
        last_dist = dist = googleTranslate(src);
    }
};

char Translate::format_buffer[Translate::MAX_BUFFER_SIZE] = {};
string Translate::select_buffer = string();
Translate::Options Translate::options = {false, 3, "auto", "zh-CN"};

int main(int argc, char** argv) {
    Translate::getOption(argc, argv);

    while(true) {
        try {
            sleep(Translate::options.delay);
            string src = "hello", dist = "你好";
            static int last_cnt = -1;
            int cnt = 0;
            Translate::translateFromSelect(src, dist, cnt);

            if(cnt == last_cnt) continue;
            last_cnt = cnt;

            if(Translate::options.clear) cout << CTRL_CLEAR << CTRL_CURINIT;
            cout << CTRL_BLOOD << "[source]:" << cnt << CTRL_RESET << endl;
            cout << src << endl;
            cout << CTRL_BLOOD << "[translated]" << CTRL_RESET << endl;
            cout << dist << endl << endl;
        } catch(Exception &e) {
            cout << "Exception:" << e.what() << endl;
            continue;
        } catch (exception &e) {
            cout << "Exception:" << e.what() << endl;
            continue;
        }
    }
    
    return 0;
}