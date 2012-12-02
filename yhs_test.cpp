#include "yhs.h"

#ifdef WIN32

// Windows
#define _WIN32_WINNT 0x500
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#endif

#ifdef __APPLE__

// Mac/iBlah
#include <unistd.h>

#endif

#include <stdlib.h>
#include <vector>

static const int PORT=35000;
static bool g_quit=false;

// Simple YHS test.
//
// Creates server on port PORT. Visit it using a web browser. Pages:
//
// /folder/ - demonstration of folder handler
//
// /file - demonstration of file handler
//
// /redir - redirects you to /file
//
// /image.png - demonstration of image creation
//
// /defer.html - demonstration of deferred bits

static void HandleFolder(yhsRequest *re,void *context)
{
    (void)context;
    
    yhs_data_response(re,"text/html");
    
    yhs_text(re,"<html><head><title>Example Folder</title></head><body><p>Handler for folder.</p></body></html>");
}

static void HandleRedir(yhsRequest *re,void *context)
{
	(void)context;

	yhs_see_other_response(re,"/file");
}

static void HandleFile(yhsRequest *re,void *context)
{
    (void)context;
    
    yhs_data_response(re,"text/plain");
    
    yhs_text(re,"Handler for individual file.");
}

static void HandleImage(yhsRequest *re,void *context)
{
    (void)context;
    
    yhs_image_response(re,512,512,3);
    
    for(size_t i=0;i<262144;++i)
    {
        size_t r=(i&63),g=(i>>6)&63,b=(i>>12)&63;
        
        yhs_pixel(re,(r<<2)|(r>>4),(g<<2)|(g>>4),(b<<2)|(b>>4),255);
    }
}

static void HandleFormHTML(yhsRequest *re,void *context)
{
	bool defer=!!context;
    
	yhs_data_response(re,"text/html");

	yhs_textf(re,"<html><head><title>Test form GET/POST%s</title></head>\n",defer?" (deferred)":"");
    yhs_text(re,"\n");
    yhs_text(re,"<!-- see http://www.w3.org/TR/REC-html40/interact/forms.html -->\n");
    yhs_text(re,"\n");
    yhs_text(re,"<body>\n");
    yhs_text(re,"\n");
	yhs_textf(re,"<FORM action=\"%s\" method=\"post\">\n",defer?"status_deferred":"status");
    yhs_text(re," <P>\n");
    yhs_text(re," <FIELDSET>\n");
    yhs_text(re,"  <LEGEND>Personal Information</LEGEND>\n");
    yhs_text(re,"  Last Name: <INPUT name=\"personal_lastname\" type=\"text\" tabindex=\"1\">\n");
    yhs_text(re,"  First Name: <INPUT name=\"personal_firstname\" type=\"text\" tabindex=\"2\">\n");
    yhs_text(re,"  Address: <INPUT name=\"personal_address\" type=\"text\" tabindex=\"3\">\n");
    yhs_text(re,"  ...more personal information...\n");
    yhs_text(re," </FIELDSET>\n");
    yhs_text(re," <FIELDSET>\n");
    yhs_text(re,"  <LEGEND>Medical History</LEGEND>\n");
    yhs_text(re,"  <INPUT name=\"history_illness\" \n");
    yhs_text(re,"         type=\"checkbox\" \n");
    yhs_text(re,"         value=\"Smallpox\" tabindex=\"20\"> Smallpox\n");
    yhs_text(re,"  <INPUT name=\"history_illness\" \n");
    yhs_text(re,"         type=\"checkbox\" \n");
    yhs_text(re,"         value=\"Mumps\" tabindex=\"21\"> Mumps\n");
    yhs_text(re,"  <INPUT name=\"history_illness\" \n");
    yhs_text(re,"         type=\"checkbox\" \n");
    yhs_text(re,"         value=\"Dizziness\" tabindex=\"22\"> Dizziness\n");
    yhs_text(re,"  <INPUT name=\"history_illness\" \n");
    yhs_text(re,"         type=\"checkbox\" \n");
    yhs_text(re,"         value=\"Sneezing\" tabindex=\"23\"> Sneezing\n");
    yhs_text(re,"  ...more medical history...\n");
    yhs_text(re," </FIELDSET>\n");
    yhs_text(re," <FIELDSET>\n");
    yhs_text(re,"  <LEGEND>Current Medication</LEGEND>\n");
    yhs_text(re,"  Are you currently taking any medication? \n");
    yhs_text(re,"  <INPUT name=\"medication_now\" \n");
    yhs_text(re,"         type=\"radio\" \n");
    yhs_text(re,"         value=\"Yes\" tabindex=\"35\">Yes\n");
    yhs_text(re,"  <INPUT name=\"medication_now\" \n");
    yhs_text(re,"         type=\"radio\" \n");
    yhs_text(re,"         value=\"No\" tabindex=\"35\">No\n");
    yhs_text(re,"\n");
    yhs_text(re,"  If you are currently taking medication, please indicate\n");
    yhs_text(re,"  it in the space below:\n");
    yhs_text(re,"  <TEXTAREA name=\"current_medication\" \n");
    yhs_text(re,"            rows=\"20\" cols=\"50\"\n");
    yhs_text(re,"            tabindex=\"40\">\n");
    yhs_text(re,"  </TEXTAREA>\n");
    yhs_text(re," </FIELDSET>\n");
    yhs_text(re,"\n");
    yhs_text(re,"<INPUT type=\"submit\" name=\"submit\">\n");
    yhs_text(re,"\n");
    yhs_text(re,"</FORM>\n");
    yhs_text(re,"\n");
    yhs_text(re,"\n");
    yhs_text(re,"</body>\n");
    yhs_text(re,"</html>\n");
}

static void HandleStatus(yhsRequest *re,void *context)
{
	(void)context;

    if(yhs_read_form_content(re))
    {
        printf("%s: %u controls:\n",__FUNCTION__,unsigned(yhs_get_num_controls(re)));
        
        for(size_t i=0;i<yhs_get_num_controls(re);++i)
        {
            printf("%u. Name: \"%s\"\n",(unsigned)i,yhs_get_control_name(re,i));
            printf("    Value:\n---8<---\n");
            printf("%s\n",yhs_get_control_value(re,i));
            printf("\n---8<---\n");
        }
    }

	yhs_see_other_response(re,"form.html");
}

struct Deferred
{
    unsigned when;
    yhsRequest *dre;
	void (*fn)(yhsRequest *);

	Deferred(unsigned when_a,yhsRequest *re,void (*fn_a)(yhsRequest *)):
	when(when_a),
	dre(yhs_defer_response(re)),
	fn(fn_a)
	{
	}
};

static std::vector<Deferred> g_deferreds;
static unsigned g_now;

static void DeferredImage(yhsRequest *re)
{
	yhs_image_response(re,64,64,3);
	for(int i=0;i<64*64;++i)
		yhs_pixel(re,rand()&0xFF,rand()&0xFF,rand()&0xFF,255);
}

static void DeferImage(yhsRequest *re,void *context)
{
	g_deferreds.push_back(Deferred(g_now+int(size_t(context))*100,re,&DeferredImage));
}

static void DeferHTML(yhsRequest *re,void *context)
{
    (void)context;
    
    yhs_data_response(re,"text/html");
    
    yhs_text(re,"<html><head><title>Deferred Responses</title></head><body>");
    
    yhs_text(re,"<p>1 <img src=\"1.png\"></p>");
    yhs_text(re,"<p>2 <img src=\"2.png\"></p>");
    yhs_text(re,"<p>3 <img src=\"3.png\"></p>");
    yhs_text(re,"<p>4 <img src=\"4.png\"></p>");
    yhs_text(re,"<p>5 <img src=\"5.png\"></p>");
    yhs_text(re,"<p>6 <img src=\"6.png\"></p>");
    yhs_text(re,"<p>7 <img src=\"7.png\"></p>");
    yhs_text(re,"<p>8 <img src=\"8.png\"></p>");
    yhs_text(re,"<p>9 <img src=\"9.png\"></p>");
    
    yhs_text(re,"</body></html>");
}

static void HandleTerminate(yhsRequest *re,void *context)
{
	(void)re,(void)context;

	g_quit=true;
}

#ifdef WIN32
static void WaitForKey()
{
    if(IsDebuggerPresent())
    {
        fprintf(stderr,"press enter to exit.\n");
        getchar();
    }
}
#endif

int main()
{
#ifdef WIN32
    atexit(&WaitForKey);
    
    WSADATA wd;
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        fprintf(stderr,"FATAL: failed to initialize Windows Sockets.\n");
        return EXIT_FAILURE;
    }
#endif

#ifdef _MSC_VER
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG)|_CRTDBG_LEAK_CHECK_DF|_CRTDBG_CHECK_ALWAYS_DF);
#endif
    
    yhsServer *server=yhs_new_server(PORT);
    
    if(!server)
    {
        fprintf(stderr,"FATAL: failed to start server.\n");
        return EXIT_FAILURE;
    }

	yhs_set_server_name(server,"Demo Server");
    
    yhs_add_to_toc(yhs_add_res_path_handler(server,"/folder/",&HandleFolder,0));
	yhs_add_to_toc(yhs_add_res_path_handler(server,"/file",&HandleFile,0));
    yhs_add_to_toc(yhs_add_res_path_handler(server,"/redir",&HandleRedir,0));
    yhs_add_to_toc(yhs_add_res_path_handler(server,"/image.png",&HandleImage,0));
    yhs_add_res_path_handler(server,"/1.png",&DeferImage,(void *)1);
    yhs_add_res_path_handler(server,"/2.png",&DeferImage,(void *)2);
    yhs_add_res_path_handler(server,"/3.png",&DeferImage,(void *)3);
    yhs_add_res_path_handler(server,"/4.png",&DeferImage,(void *)4);
    yhs_add_res_path_handler(server,"/5.png",&DeferImage,(void *)5);
    yhs_add_res_path_handler(server,"/6.png",&DeferImage,(void *)6);
    yhs_add_res_path_handler(server,"/7.png",&DeferImage,(void *)7);
    yhs_add_res_path_handler(server,"/8.png",&DeferImage,(void *)8);
    yhs_add_res_path_handler(server,"/9.png",&DeferImage,(void *)9);
    yhs_add_to_toc(yhs_add_res_path_handler(server,"/defer.html",&DeferHTML,0));
	yhs_add_to_toc(yhs_add_res_path_handler(server,"/form.html",&HandleFormHTML,(void *)0));
    yhs_set_handler_description("form with deferred response",yhs_add_to_toc(yhs_add_res_path_handler(server,"/form.html",&HandleFormHTML,(void *)1)));
    yhs_add_res_path_handler(server,"/status",&HandleStatus,0);
	yhs_add_to_toc(yhs_add_res_path_handler(server,"/terminate",&HandleTerminate,0));

    while(!g_quit)
    {
        yhs_update(server);
        
#ifdef WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
        
        ++g_now;
        
        std::vector<Deferred>::iterator it=g_deferreds.begin();
        while(it!=g_deferreds.end())
        {
            if(g_now>=it->when)
            {
				(*it->fn)(it->dre);

                yhs_end_deferred_response(it->dre);
                
                it=g_deferreds.erase(it);
            }
            else
                ++it;
        }
    }

	yhs_delete_server(server);
	server=0;

// #ifdef _MSC_VER
// 	_CrtDumpMemoryLeaks();
// #endif

	return EXIT_SUCCESS;
}