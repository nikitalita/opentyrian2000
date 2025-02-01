#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <memory.h>
#include <SDL2/SDL_mixer_ext.h>
#include <midiproc/midiproc.h>

typedef struct _MidiData {
	uint8_t *data;
	uint32_t size;
	uint32_t duration;
	uint32_t loop_start;
	uint32_t loop_end;
	uint32_t track_count;
	uint32_t subsong_count;
    uint32_t duration_ms;
} MidiData;
static MidiData * midi_data;
static Mix_Music ** midi_tracks = NULL;

static FILE *music_file = NULL;
static uint32_t *song_offset;
static uint16_t song_count = 0;

void convert_midi_data(void){
    // initialize the midi_data array
    midi_data = malloc(song_count * sizeof(*midi_data));
    midi_tracks = malloc(song_count * sizeof(*midi_tracks));
    memset(midi_tracks, 0, song_count * sizeof(*midi_tracks));
    for (unsigned int i = 0; i < song_count; ++i)
    {
        memset(&midi_data[i], 0, sizeof(MidiData));
        uint32_t start = song_offset[i];
        uint32_t end = song_offset[i + 1];
        uint32_t size = end - start;
        uint8_t *buf = malloc(size);
        FILE *midfile = NULL;
        char midfilename[4096];

        fread(buf, size, 1, music_file);
        HMIDIContainer midi_container = MIDPROC_Container_Create();
        if (!MIDPROC_Process(buf, size, "lds", midi_container))
        {
            fprintf(stderr, "warning: failed to process song %d\n", i + 1);
            MIDPROC_Container_Delete(midi_container);
            free(buf);
            continue;
        }
        size_t midi_data_size = 0;
        MIDPROC_Container_SerializeAsSMF(midi_container, &(midi_data[i].data), &midi_data_size);
        midi_data[i].size = (Uint32)midi_data_size;
        if (midi_data[i].size == 0)
        {
            fprintf(stderr, "warning: failed to process song %d\n", i + 1);
            continue;
        }

        snprintf(midfilename, sizeof(midfilename), "music%d.mid", i);
        midfile = fopen(midfilename, "wb");
        fwrite(midi_data[i].data, midi_data[i].size, 1, midfile);
        fclose(midfile);

        midi_data[i].duration = MIDPROC_Container_GetDuration(midi_container, 0, false);
        midi_data[i].duration_ms = MIDPROC_Container_GetDuration(midi_container, 0, true);
        MIDPROC_Container_DetectLoops(midi_container, false, true, false, false);
        midi_data[i].loop_start = MIDPROC_Container_GetLoopBeginTimestamp(midi_container, 0, false);
        midi_data[i].loop_end = MIDPROC_Container_GetLoopEndTimestamp(midi_container, 0, false);
        midi_data[i].track_count = MIDPROC_Container_GetTrackCount(midi_container);
        midi_data[i].subsong_count = MIDPROC_Container_GetSubSongCount(midi_container);
        MIDPROC_Container_Delete(midi_container);

        free(buf);
    }
}

int main(void)
{
    size_t fileoffset = 0;

    music_file = fopen("music.mus", "rb");
    fread(&song_count, sizeof(uint16_t), 1, music_file);
    song_offset = malloc((song_count + 1) * sizeof(*song_offset));
    fread(song_offset, sizeof(uint32_t), song_count, music_file);
    fileoffset = ftell(music_file);
    fseek(music_file, 0, SEEK_END);
    song_offset[song_count] = ftell(music_file);
    fseek(music_file, fileoffset, SEEK_SET);
    convert_midi_data();

	return 0;
}
