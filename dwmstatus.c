#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <mpd/client.h>
#include <mpd/tag.h>

#include <X11/Xlib.h>

#define MPDHOST "localhost"
#define MPDPORT 6600

char *tzbuc = "Europe/Bucharest";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
    va_list fmtargs;
    char *ret;
    int len;

    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);

    ret = malloc(++len);
    if (ret == NULL) {
        perror("malloc");
        exit(1);
    }

    va_start(fmtargs, fmt);
    vsnprintf(ret, len, fmt, fmtargs);
    va_end(fmtargs);

    return ret;
}

void
settz(char *tzname)
{
    setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
    char buf[129];
    time_t tim;
    struct tm *timtm;

    bzero(buf, sizeof(buf));
    settz(tzname);
    tim = time(NULL);
    timtm = localtime(&tim);
    if (timtm == NULL) {
        perror("localtime");
        exit(1);
    }

    if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
        fprintf(stderr, "strftime == 0\n");
        exit(1);
    }

    return smprintf("%s", buf);
}

void
setstatus(char *str)
{
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

char *
loadavg(void)
{
    double avgs[3];

    if (getloadavg(avgs, 3) < 0) {
        perror("getloadavg");
        exit(1);
    }

    return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
getbattery(char *base)
{
    //TODO: return NULL if plugged in
    char *path, line[513];
    FILE *fd;
    int descap, remcap;

    descap = -1;
    remcap = -1;

    path = smprintf("%s/info", base);
    fd = fopen(path, "r");
    if (fd == NULL) {
        free(path);
        /*perror("fopen");*/
        /*exit(1);*/
        return NULL;
    }
    free(path);
    while (!feof(fd)) {
        if (fgets(line, sizeof(line)-1, fd) == NULL)
            break;

        if (!strncmp(line, "present", 7)) {
            if (strstr(line, " no")) {
                descap = 1;
                break;
            }
        }
        if (!strncmp(line, "design capacity", 15)) {
            if (sscanf(line+16, "%*[ ]%d%*[^\n]", &descap))
                break;
        }
    }
    fclose(fd);

    path = smprintf("%s/state", base);
    fd = fopen(path, "r");
    if (fd == NULL) {
        free(path);
        perror("fopen");
        exit(1);
    }
    free(path);
    while (!feof(fd)) {
        if (fgets(line, sizeof(line)-1, fd) == NULL)
            break;

        if (!strncmp(line, "present", 7)) {
            if (strstr(line, " no")) {
                remcap = 1;
                break;
            }
        }
        if (!strncmp(line, "remaining capacity", 18)) {
            if (sscanf(line+19, "%*[ ]%d%*[^\n]", &remcap))
                break;
        }
    }
    fclose(fd);

    if (remcap < 0 || descap < 0)
        return NULL;

    return smprintf("%.0f", ((float)remcap / (float)descap) * 100);
}

/**
 * Get the current RAM usage as a percentage
 *
 * @return float a number representing what proportion of the RAM is in use
 * eg: 42.3 meaning that 42.3% of the RAM is used
 */
float getram(){
    int total, free, buffers, cached;
    FILE *f;

    f = fopen("/proc/meminfo", "r");

    if(f == NULL){
        perror("fopen");
        exit(1);
    }

    // MemTotal and MemFree reside on the first two lines of /proc/meminfo
    fscanf(f, "%*s %d %*s %*s %d %*s %*s %d %*s %*s %d", &total, &free,
            &buffers, &cached);
    fclose(f);

    return (float)(total-free-buffers-cached)/total * 100;
}

/**
 * Get the number of core the CPU has
 *
 * @return int the number of cores
 */
int getnumcores(){
    FILE *f;
    char line[513];
    int numcores = 0;
    f = fopen("/proc/cpuinfo", "r");

    while(!feof(f) && fgets(line, sizeof(line)-1, f) != NULL){
        if(strstr(line, "processor")){
            numcores++;
        }
    }

    fclose(f);

    return numcores;
}

/**
 * Get the current (per core) CPU load
 * http://stackoverflow.com/a/3017332/770023
 *
 * @param int numcores the number of cores the current CPU has
 *
 * @return The return value is an int representing a percentage of one core load
 * eg: 42 which means 42% of one core is used and 84% of the whole CPU is used
 *
 * TODO: use /proc/stat
 */
int getcpu(int numcores){
    double load[1];

    if (getloadavg(load, 1) < 0) {
        perror("getloadavg");
        exit(1);
    }

    return (int)(load[0]/numcores * 100)%100;
}

/**
 * Get the current swap usage as a percentage
 *
 * @return float a number representing what proportion of the swap is in use
 * eg: 42.3 meaning that 42.3% of the swap is used
 */
float getswap(){
    char line[513];
    int total = -1, free = -1;
    FILE *f;

    f = fopen("/proc/meminfo", "r");

    if(f == NULL){
        perror("fopen");
        exit(1);
    }

    while(!feof(f) && fgets(line, sizeof(line)-1, f) != NULL
            && (total == -1 || free == -1)){
        if(strstr(line, "SwapTotal")){
            sscanf(line, "%*s %d", &total);
        }

        if(strstr(line, "SwapFree")){
            sscanf(line, "%*s %d", &free);
        }
    }

    fclose(f);

    return (float)(total-free)/total * 100;
}


/**
 * Replace a string with a formatted one
 *
 * @param char **str the string to be replaced
 * @param char *fmt the format of the replacement string
 * @param ... a list of variables to be formatted
 *
 * @return int the number characters in the final string or a negative if errors
 * occured (in which case str is unchanged)
 */
int srprintf(char **str, char *fmt, ...){
    va_list fmtargs;
    char *replacement;
    int len;

    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);

    // tried realloc here, but since it can resize the memory block whithout
    // changing the location the original contents of the str could be lost if
    // str itself was sent as an argument to be formatted (ie: in fmtargs)
    replacement = (char *) malloc(++len);

    if(replacement == NULL){
        perror("malloc");
        return -1;
    }

    va_start(fmtargs, fmt);
    vsnprintf(replacement, len, fmt, fmtargs);
    va_end(fmtargs);

    free(*str);
    *str = replacement;

    return len;
}

int remove_ext(char *str){
    char *dot = strrchr(str, '.');

    if(dot){
        *dot = '\0';
        return dot-str+1;
    }

    return -1;
}

char *get_filename(const char *str){
    char *dir_sep = strrchr(str, '/');

    if(dir_sep == NULL){
        return (char *)str;
    }

    return dir_sep+1;
}

char *
getmpd() {
    struct mpd_connection *conn;
    struct mpd_song *song;
    struct mpd_status *status;
    const char *artist;
    const char *title;
    const char *name;
    char *retval;

    conn = mpd_connection_new(MPDHOST,MPDPORT, 30000);

    if(mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS){
        fprintf(stderr, "%s", mpd_connection_get_error_message(conn));
        mpd_connection_free(conn);

        return NULL;
    }

    status = mpd_run_status(conn);

    if(status == NULL){
        fprintf(stderr, "Cannot get MPD status!\n");
        mpd_connection_free(conn);
        return NULL;
    }

    enum mpd_state state = mpd_status_get_state(status);

    if(state == MPD_STATE_STOP || state == MPD_STATE_UNKNOWN){
        mpd_status_free(status);
        mpd_connection_free(conn);

        return NULL;
    }
    else if(state == MPD_STATE_PAUSE){
        mpd_status_free(status);
        mpd_connection_free(conn);
        return smprintf("paused");
    }

    song = mpd_run_current_song(conn);

    if(song == NULL){
        fprintf(stderr, "Error fetching current song!\n");
        mpd_status_free(status);
        mpd_connection_free(conn);

        return NULL;
    }

    artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
    title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
    name = mpd_song_get_tag(song, MPD_TAG_NAME, 0);

    mpd_status_free(status);
    mpd_connection_free(conn);

    if(title){
        if(artist){
            retval = smprintf("%s - %s", artist, title);
        }
        else{
            retval = smprintf("%s", title);
        }

        mpd_song_free(song);
        return retval;
    }

    if(name){
        retval = smprintf("%s", name);

        mpd_song_free(song);
        return retval;
    }

    const char *uri = mpd_song_get_uri(song);

    remove_ext((char *)uri);
    uri = get_filename(uri);

    retval = smprintf("%s", uri);

    mpd_song_free(song);
    return retval;
}

int
main(void)
{
    //TODO: what happens with avgs, bat, etc if I exit at an exit(1) aka: FREE
    //THEM!
    //TODO: current network usage, weather stats, current mpd song, computer
    //temperature, vol, check: https://code.google.com/p/dwm-hacks/
    char *status;
    char *avgs;
    char *bat;
    char *tmbuc;
    char *mpd;

    float swap;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "dwmstatus: cannot open display.\n");
        return 1;
    }

    int numcores = getnumcores();

    for (;;sleep(1)) {
        avgs = loadavg();
        bat = getbattery("/proc/acpi/battery/BAT0");
        tmbuc = mktimes("%d-%m-%Y %R", tzbuc);
        mpd = getmpd();

        swap = getswap();

        status = smprintf("[");

        if(mpd != NULL){
            srprintf(&status, "%s%s • ", status, mpd);
        }

        srprintf(&status, "%sram: %0.f%% • cpu: %d%%", status, getram(), getcpu(numcores));

        if(swap >= 1){
            srprintf(&status, "%s • swap: %.0f%%", status, swap);
        }

        if(bat != NULL){
            srprintf(&status, "%s • bat: %s%%", status, bat);
        }

        srprintf(&status, "%s • %s]", status, tmbuc);

        setstatus(status);

        free(avgs);
        free(bat);
        free(tmbuc);
        free(mpd);
        free(status);
    }

    XCloseDisplay(dpy);

    return 0;
}
