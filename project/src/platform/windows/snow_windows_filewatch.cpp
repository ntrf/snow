
#include "snow_core.h"
#include "snow_io.h"


#include <string>
#include <vector>

#include <windows.h>

namespace snow {

    extern double timestamp();

    namespace io {


        class FileWatcherThread {
          public:
            std::string path;
            HANDLE handle;
            HANDLE thread;
            OVERLAPPED overlap;
            BYTE* buffer;
            size_t buffer_size;

            static DWORD WINAPI run_thread( void *watcher );

            FileWatcherThread(const std::string &_path) {
                path = std::string(_path);
                buffer_size = 1024;
                buffer = new BYTE[buffer_size];

                handle = ::CreateFile(
                    path.c_str(),
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                    NULL
                );

                if(handle != INVALID_HANDLE_VALUE) {

                    ZeroMemory(&overlap, sizeof(overlap));
                    overlap.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

                    DWORD threadid;
                    thread = CreateThread(NULL, 0, &run_thread, this, 0, &threadid);

                } //!invalid handle

            } //construct

            ~FileWatcherThread() {
                ::CancelIo(handle);
                ::CloseHandle(handle);
                ::CloseHandle(thread);
            }

        };

        DWORD WINAPI FileWatcherThread::run_thread( void *_watcher ) {

                FileWatcherThread *watcher = (FileWatcherThread*)_watcher;

                ReadDirectoryChangesW(
                    watcher->handle, watcher->buffer, watcher->buffer_size, TRUE,
                    FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                    NULL, &watcher->overlap, NULL
                );

                WaitForSingleObject(watcher->overlap.hEvent, INFINITE);

                    int seek = 0;

                    while(seek < watcher->buffer_size) {

                        PFILE_NOTIFY_INFORMATION notifier = PFILE_NOTIFY_INFORMATION(watcher->buffer + seek);

                        WCHAR szwFileName[MAX_PATH];
                        int ulCount = notifier->FileNameLength/2;
                        wcsncpy(szwFileName, notifier->FileName, ulCount);
                        szwFileName[ulCount] = L'\0';

                        std::wstring widepath(szwFileName);
                        std::string path(widepath.begin(), widepath.end());

                        printf("event %d\n", notifier->Action);

                        FileWatchEventType _event_type = fe_unknown;

                            switch( notifier->Action ) {

                                case FILE_ACTION_ADDED:{
                                    _event_type = fe_create;
                                    break;
                                }

                                case FILE_ACTION_REMOVED:{
                                    _event_type = fe_remove;
                                    break;
                                }

                                case FILE_ACTION_MODIFIED:{
                                    _event_type = fe_modify;
                                    break;
                                }

                                case FILE_ACTION_RENAMED_OLD_NAME:{
                                    _event_type = fe_remove;
                                    break;
                                }
                                
                                case FILE_ACTION_RENAMED_NEW_NAME:{
                                    _event_type = fe_create;
                                    break;
                                }
                                
                            } //switch

                            if(_event_type != fe_unknown) {
                                FileWatchEvent event( _event_type, snow::timestamp(), (watcher->path+"/"+path) );
                                snow::io::dispatch_event( event );
                            }

                        seek += notifier->NextEntryOffset;

                        if(notifier->NextEntryOffset == 0) {
                            break;
                        }

                    } //seek < buffer size

                //done loop, but run it again
                run_thread(_watcher);

                return 0;

            } //run_thread




        std::vector<std::string> watched_paths;
        std::vector<FileWatcherThread*> watchers;

        bool init_filewatch() {

            snow::log("/ snow / initialized file watch ok");

            return true;

        } //init_filewatch

        void start_filewatch(){

            if(watched_paths.size() == 0) {
                return;
            }

            std::vector<std::string>::iterator it = watched_paths.begin();

            for( ; it != watched_paths.end(); ++it) {

                std::string path = *it;

                FileWatcherThread* watcher = new FileWatcherThread(path);
                watchers.push_back(watcher);

            } //each watched path

            snow::log("/ snow / filewatch started");

        } //start_filewatch

        void stop_filewatch() {

            if(watchers.size() == 0) {
                return;
            }

            std::vector<FileWatcherThread*>::iterator it = watchers.begin();

            for( ; it != watchers.end(); ++it) {
                FileWatcherThread* watcher = *it;
                if(watcher) { 
                    delete watcher;
                }
            }

            watchers.clear();

            snow::log("/ snow / filewatch stopped");

        } //stop_filewatch

        void refresh_filewatch() {

            stop_filewatch();
            start_filewatch();

        } //refresh_filewatch

            //not used, as these run in a thread on windows
            //per directory currently.
        void update_filewatch() {
        }

        void shutdown_filewatch() {

            stop_filewatch();

            snow::log("/ snow / filewatch shutdown");

        } //shutdown_filewatch

    } //io namespace

} //snow namespace

