#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <codecvt>
#include <cstring>
#include <locale>
#include <getopt.h>
#include <csignal>
#include <unistd.h>
#include <curl/curl.h>
#include <openssl/md5.h>
#include "cJSON/cJSON.h"
using namespace std;

#define CTRL_RED "\033[31m"
#define CTRL_GEEEN "\033[32m"
#define CTRL_RESET "\033[0m"
#define CTRL_BLOOD "\033[1m"
#define CTRL_CLEAR "\033[2J"
#define CTRL_CURINIT "\033[1;1H"

// baidu app id and key, replace them to your owns
#define BAIDU_APP_ID "12345678"
#define BAIDU_APP_KEY "12345678"

const char HELP_MSG[]= 
    "usage:\n"
    "    select-translate [options]\n"
    "options:\n"
    "    -c             --clear             clean screen before translation\n"
    "    -d <time/s>    --delay             the time between two translation (default: 3)\n"
    "    -e <site>      --engine            translate engine (default: google)\n"
    "                                       [baidu/google]\n"
    "    -s <lang>      --source-language   source language. (default: auto)\n"
    "                                       [Chinese/English/Japanese/Chinese_Simplified\n"
    "                                        /Chinese_traditional/auto]\n"
    "    -t <lang>      --target-language   target language. (default: Chinese)\n"
    "                                       [Chinese/English/Japanese/Chinese_Simplified\n"
    "                                        /Chinese_traditional]\n"
    "    -m <mode>      --mode              set text from clipboard or selection (default:\n"
    "                                       selection)\n"
    "                                       [clipboard/selection]\n"
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
        // translate api, baidu or google
        string engine;
        // source  language
        string sl;
        // target language
        string tl;
        // text from clipboard(true) or select(false)
        bool mode;
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
            {"engine", required_argument, NULL, 'e'},
            {"source-language", required_argument, NULL, 's'},
            {"target-language", required_argument, NULL, 't'},
            {"mode", required_argument, NULL, 'm'},
            {"help", no_argument, NULL, 'h'},
            {0, 0, 0, 0}
        };

        map<string,string> lang_map_google = {
            {"Chinese", "zh-CN"},
            {"English", "en"},
            {"Japanese", "ja"},
            {"Chinese_Simplified", "zh-CN"},
            {"Chinese_Traditional", "zh-TW"},
            {"auto", "auto"}
        };

        map<string,string> lang_map_baidu = {
            {"Chinese", "zh"},
            {"English", "en"},
            {"Japanese", "jp"},
            {"Chinese_Simplified", "zh"},
            {"Chinese_Traditional", "zht"},
            {"auto", "auto"}
        };

        string src_lang;
        string dist_lang;
        while ((opt = getopt_long(argc, argv, "cd:e:s:l:hm:", long_options, &loidx)) != -1) {
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
                case 'e':
                    arg_string = string(optarg);
                    if(arg_string == "baidu") options.engine = "baidu";
                    else if (arg_string == "google") options.engine = "google";
                    else {
                        cout << "unkown engine: " << arg_string << ". using default: google" << endl;
                        options.engine = "google";
                    }
                    break;
                case 's':
                    src_lang = string(optarg);
                    break;
                case 't':
                    dist_lang = string(optarg);
                    break;
                case 'h':
                    cout << HELP_MSG << endl;
                    exit(0);
                    break;
                case 'm':
                    arg_string = string(optarg);
                    if (arg_string == "clipboard") options.mode = true;
                    else options.mode = false;
                    break;
                default:
                    break;
            }
        }

        if(options.engine == "google") {
            if(lang_map_google.find(src_lang) == lang_map_google.end()) options.sl = "auto";
                else options.sl = lang_map_google[src_lang];
            if(lang_map_google.find(dist_lang) == lang_map_google.end()) options.tl = lang_map_google["Chinese"];
                else options.tl = lang_map_google[dist_lang];
        } else {
            if(lang_map_baidu.find(src_lang) == lang_map_baidu.end()) options.sl = "auto";
                else options.sl = lang_map_baidu[src_lang];
            if(lang_map_baidu.find(dist_lang) == lang_map_baidu.end()) options.tl = lang_map_baidu["Chinese"];
                else options.tl = lang_map_baidu[dist_lang];
        }


        string mode;
        if(options.mode) mode = "clipboard";
        else mode = "selection";

        cout << "settings:" << endl
             << "clear:           " << (options.clear ? "true" : "false") << endl
             << "delay:           " << options.delay << endl
             << "engine:          " << options.engine << endl
             << "mode:            " << mode << endl
             << "source language: " << options.sl << endl
             << "target language: " << options.tl << endl
             << "\nstart" << endl;
        sleep(5);
    }

    static void getSelect(bool mode) {
        string xsel = string("xsel") + (mode ? " -b" : "");
        char temp_buffer[101];
        int filled_size = 0;

        // reset select buffer
        Translate::select_buffer.clear();

        // get "xsel" output
        FILE* output_stream = nullptr;
        if((output_stream = popen(xsel.c_str(), "r"))) {
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
                    throw Exception("error: failed to get google transalte result");
                }

                res += cJSON_GetStringValue(str1);
            }

            cJSON_Delete(json);

            return res;
        } else {
            throw Exception("error: failed to GET translation");
        }
    # else
        return src;
    # endif
    }

    static string filter(string src) {
        vector<string> from = {"\n", "\t", "  "};
        vector<string> to = {" ", " "," "};

        for(size_t i = 0; i < from.size(); i++) {
            string f = from[i], t = to[i];
            size_t idx;
            while((idx = src.find(f)) != string::npos) {
                src.replace(idx, f.size(), t);
            }
        }

        return src;
    }

    static string baiduTranslate(string src) {
        string url = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
        string param = "appid=%s&q=%s&from=%s&to=%s&salt=%s&sign=%s";

        string appid = BAIDU_APP_ID;
        string secret_key = BAIDU_APP_KEY;

        string sl = options.sl;
        string tl = options.tl;

        // calculate md5 sum
        char salt[60];
        sprintf(salt, "%d", rand());
        string sign;
        sign += appid;
        sign += src;
        sign += salt;
        sign += secret_key;
        
        uint8_t mdhex[16];
        string mdreadable;
        char tmp[3]={'\0'};
        MD5((const uint8_t *)sign.c_str(), sign.size(),mdhex);
        for (int i = 0; i < 16; i++){
            sprintf(tmp,"%2.2x",mdhex[i]);
            mdreadable += tmp;
        }

        src = CodeConvert::urlEncode(src);

        string format = url + param;
        sprintf(Translate::format_buffer, format.c_str(), 
            appid.c_str(),
            src.c_str(),
            sl.c_str(),
            tl.c_str(),
            salt,
            mdreadable.c_str()
        );

        #ifdef DEBUG
            cout << Translate::format_buffer << endl;
            return src;
        #endif

        string response;
        int sta = curlGet(Translate::format_buffer, &response);

        if(sta != -1) {
            string res;
            cJSON *json = cJSON_Parse(response.c_str());
            cJSON *result = cJSON_GetObjectItem(json,"trans_result");

            if(result == NULL) {
                cJSON_Delete(json);
                throw Exception("error: failed to get baidu transalte result");
            }

            int size = cJSON_GetArraySize(result);
            for(int i = 0; i < size;i++) {
                cJSON *obji = cJSON_GetArrayItem(result, i);
                cJSON *str1 = cJSON_GetObjectItem(obji,"dst");

                if(str1 == NULL) {
                    cJSON_Delete(json);
                    throw Exception("error: failed to get baidu transalte result");
                }

                res += cJSON_GetStringValue(str1);
            }

            cJSON_Delete(json);

            return res;
        } else {
            throw Exception("error: failed to GET translation");
        }     
    }

    static void translate(string &src, string &dist, int &count) {
        static string last_src;
        static string last_dist;
        static int cnt = 0;

        Translate::getSelect(Translate::options.mode);
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

        if(options.engine == "google")
            last_dist = dist = googleTranslate(src);
        else
            last_dist = dist = baiduTranslate(src);
    }
};

char Translate::format_buffer[Translate::MAX_BUFFER_SIZE] = {};
string Translate::select_buffer = string();
Translate::Options Translate::options = {false, 3, "google" ,"auto", "zh-CN", false};

void signalHandler(int sig) {
    if(sig == SIGINT) {
        cout << endl << "Receive SIGINT. Exit." << endl;
        exit(0);
    }
}

int main(int argc, char** argv) {
    Translate::getOption(argc, argv);
    signal(SIGINT,signalHandler);
    while(true) {
        try {
            sleep(Translate::options.delay);
            string src = "hello", dist = "你好";
            static int last_cnt = -1;
            int cnt = 0;
            Translate::translate(src, dist, cnt);

            if(cnt == last_cnt) continue;
            last_cnt = cnt;

            if(Translate::options.clear) cout << CTRL_CLEAR << CTRL_CURINIT;
            cout << CTRL_BLOOD << "[source language: " << Translate::options.sl <<  "]:" << cnt << CTRL_RESET << endl;
            cout << src << endl << endl;
            cout << CTRL_BLOOD << "[target language: " << Translate::options.tl << "]" << CTRL_RESET << endl;
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