#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <vector>

//http://www.ccarh.org/courses/253/handout/smf/
static void swap2(uint8_t *p) {
	uint8_t a = p[0];
	uint8_t b = p[1];
	p[0] = b;
	p[1] = a;
}
static void swap4(uint8_t *p) {
	uint8_t a = p[0];
	uint8_t b = p[1];
	uint8_t c = p[2];
	uint8_t d = p[3];
	p[0] = d;
	p[1] = c;
	p[2] = b;
	p[3] = a;
}

#pragma pack(push, 1)
struct MThd {
	union {
		uint8_t header_bytes[4];
		uint32_t header;
	};
	union {
		uint8_t length_bytes[4];
		uint32_t length;
	};
	union {
		uint8_t format_bytes[2];
		uint16_t format;
	};
	union {
		uint8_t number_of_track_bytes[2];
		uint16_t number_of_track;
	};
	union {
		uint8_t division_bytes[2];
		uint16_t division;
	};
	void Swap() {
		swap4(length_bytes);
		swap2(format_bytes);
		swap2(number_of_track_bytes);
		swap2(division_bytes);
	}
};

struct MTrk {
	union {
		uint8_t header_bytes[4];
		uint32_t header;
	};
	union {
		uint8_t length_bytes[4];
		uint32_t length;
	};
	void Swap() {
		swap4(length_bytes);
	}
	uint8_t data[];
};
#pragma pack(pop)

struct File {
	std::vector<uint8_t> buf;
	File(const char *name) {
		FILE *fp = fopen(name, "rb");
		if(fp) {
			fseek(fp, 0, SEEK_END);
			size_t size = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			buf.resize(size);
			fread(buf.data(), 1, size, fp);
			fclose(fp);
		}
	}
	size_t GetSize() {
		return buf.size();
	}
	void *GetData() {
		return buf.data();
	}
};

int main(int argc, char *argv[]) {
	File file(argv[1]);
	if(file.GetSize() > 0) {
		if(false) {
			char *data = (char *)file.GetData();
			for(int i = 0 ; i < file.GetSize(); i++) {
				if( (i % 16) == 0) printf("\n");
				int c = data[i];
				if(isgraph(c)) {
					printf("%c", c);
				} else {
					printf(".", c);
				}
			}
		}
		printf("sizeof(MThd)=%d\n", sizeof(MThd));
		printf("sizeof(MTrk)=%d\n", sizeof(MTrk));
		
		char *data = (char *)file.GetData();
		char *data_end = data + file.GetSize();
		while(data < data_end) {
			MThd *mthd = (MThd *)data;
			MTrk *mtrk = (MTrk *)data;
			if(mthd->header == 0x6468544D) {
				mthd->Swap();
				printf("=======================================\n");
				printf("mthd->header=%08X\n", mthd->header);
				printf("mthd->length=%08X\n", mthd->length);
				printf("mthd->format=%08X\n", mthd->format);
				printf("mthd->number_of_track=%08X\n", mthd->number_of_track);
				printf("mthd->division=%08X\n", mthd->division);
				printf("=======================================\n");
				data += sizeof(MThd);
				continue;
			}

			if(mtrk->header == 0x6B72544D) {
				mtrk->Swap();
				printf("mtrk->header=%08X\n", mtrk->header);
				printf("mtrk->length=%08X\n", mtrk->length);
				uint8_t event_type_before = 0; //for running status
				for(int index = 0 ; index < mtrk->length;) {
					auto getdeltatime = [&]() {
						uint32_t ret = 0;
						uint8_t dtime = 0;
						do {
							ret <<= 7;
							dtime = mtrk->data[index];
							ret |= (dtime & 0x7F);
							index++;
						} while(dtime & 0x80);
						return ret;
					};
					printf("\nBEGIN\n");
					uint32_t delta_time = getdeltatime();
					uint8_t event_type = mtrk->data[index++];

					//00 BB 07 6E
					//00    5B 28
					//00    0B 7F
					//
					bool running = false;
					if((event_type & 0x80) == 0) {
						event_type = event_type_before;
						running = true;
						index--;
					} else {
						event_type_before = event_type;
					}
					uint8_t event_kind = event_type & 0xF0;
					uint8_t channel = event_type & 0x0F;
					printf("%s event_type=%02X, event_kind=%02X, channel=%02X delta_time=%d\n", running ? "--" : "@@", event_type, event_kind, channel, delta_time);
					printf("   ");
					
					//sysex
					if(event_type == 0xF0) {
						while(mtrk->data[index++] != 0xF7);
						printf("Meta sysex\n");
						continue;
					}
					//meta
					if(event_type == 0xFF) {
						uint8_t kind = mtrk->data[index++];
						uint32_t length = getdeltatime();
						printf("kind=%02X\n", kind);
						if(kind >= 0x01 && kind <= 0x0F) {
							printf("string_length=%02X\n", length);
							while(length-- > 0) {
								char c = mtrk->data[index++];
								printf("%c", c);
							}
							printf("\n");
							continue;
						}

						//tempo
						if(kind == 0x51) {
							uint32_t tempo = 0;
							tempo = (tempo << 8) | mtrk->data[index++];
							tempo = (tempo << 8) | mtrk->data[index++];
							tempo = (tempo << 8) | mtrk->data[index++];
							printf("TEMPO=%d[ms], tempo=%d\n", tempo / 1000, tempo);
							continue;
						}

						//time sig
						if(kind == 0x58) {
							uint32_t tsig = 0;
							printf("data=%d\n", mtrk->data[index++]);
							printf("data=%d\n", mtrk->data[index++]);
							printf("data=%d\n", mtrk->data[index++]);
							printf("data=%d\n", mtrk->data[index++]);
							printf("Time Sig=%d[ms]\n", tsig);
							continue;
						}
						

						
						printf("SysEx length=%d\n", length);
						index += length;
						continue;
					}

					//Note on
					if(event_kind == 0x80) {
						uint8_t number = mtrk->data[index++];
						uint8_t vel = mtrk->data[index++];
						printf("[Note OFF] channel=%d, delta_time=%d, no=%d, vel=%d\n", channel, delta_time, number, vel);
						continue;
					}

					//Note off
					if(event_kind == 0x90) {
						uint8_t number = mtrk->data[index++];
						uint8_t vel = mtrk->data[index++];
						printf("[Note ON ] channel=%d, delta_time=%d, no=%d, vel=%d\n", channel, delta_time, number, vel);
						continue;
					}

					//Control Change
					if(event_kind == 0xB0) {
						uint8_t kind = mtrk->data[index++];
						int8_t value = mtrk->data[index++];
							printf("Control Change kind=%d(%02X)\n", kind,kind);
						
						//volume
						if(kind == 0x07) {
							int8_t volume = value;
							printf("Control Change channel=%d, volume=%d(%02X)\n", channel, volume, volume);
						}
						
						//pan
						if(kind == 0x0A) {
							int8_t pan = value;
							printf("Control Change channel=%d, pan=%d\n", channel, pan);
						}

						//express
						if(kind == 0x0B) {
							int8_t express = value;
							printf("Control Change channel=%d, express=%d\n", channel, express);
						}
						
						continue;
					}

					//Program Change
					if(event_kind == 0xC0) {
						uint8_t kind = mtrk->data[index++];
						printf("Program Change channel=%d, kind=%d(0x%02X)\n", channel, kind, kind);
						continue;
					}
					
					//other
					{
						//uint8_t value = mtrk->data[index++];
						//printf("Unknown=%02X, value=%02X\n", event_type, value);
					}
				}
				data += sizeof(MTrk);
				continue;
			}
			data++;
		}
	}
	return 0;
}
