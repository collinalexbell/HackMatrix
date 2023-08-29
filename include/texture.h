#ifndef __TEXTURE_H__
#define __TEXTURE_H__

class Texture {
  int width, height, nrChannels;
  unsigned char *data;
 public:
  Texture(const char*);
  unsigned int ID;

};

#endif
