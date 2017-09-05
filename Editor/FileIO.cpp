#include "FileIO.h"
#include "Util.h"
#include "cJSON.h"
#include "microtar.h"
#include "fastlz.h"
#include <shlwapi.h>

bool CFileIO::Save(vector<CSavable*> savables, string &fileName)
{
  OPENFILENAME ofn;
  char szFile[260];
  
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "UltraEd (*.ultra)";
  ofn.nFilterIndex = 1;
  ofn.lpstrTitle = "Save Scene";
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_OVERWRITEPROMPT;
  
  if(GetSaveFileName(&ofn))
  {
    char *saveName = ofn.lpstrFile;

    // Add the extension if not supplied in the dialog.
    if(strstr(saveName, ".ultra") == NULL)
    {
      sprintf(saveName, "%s.ultra", saveName);
    }

    fileName = CleanFileName(saveName);

    // Prepare tar file for writing.
    mtar_t tar;
    mtar_open(&tar, saveName, "w");
    
    // Write the scene JSON data.
    cJSON *root = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "models", array);

    for(vector<CSavable*>::iterator it = savables.begin(); it != savables.end(); ++it)
    {
      Savable current = (*it)->Save();
      cJSON *object = current.object->child;

      // Add array to hold all attached resources.
      cJSON *resourceArray = cJSON_CreateArray();
      cJSON_AddItemToObject(object, "resources", resourceArray);

      // Rewrite and archive the attached resources.
      map<string, string> resources = (*it)->GetResources();
      for(map<string, string>::iterator rit = resources.begin(); rit != resources.end(); ++rit)
      {
        const char *fileName = PathFindFileName(rit->second.c_str());
        FILE *file = fopen(rit->second.c_str(), "rb");

        if(file == NULL) continue;

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, rit->first.c_str(), fileName);
        cJSON_AddItemToArray(resourceArray, item);
        
        // Calculate resource length.
        fseek(file, 0, SEEK_END);
        long fileLength = ftell(file);
        rewind(file);

        // Read all contents of resource into a buffer.
        char *fileContents = (char*)malloc(fileLength);
        fread(fileContents, fileLength, 1, file);

        // Write the buffer to the tar archive.
        mtar_write_file_header(&tar, fileName, fileLength);
        mtar_write_data(&tar, fileContents, fileLength);

        fclose(file);
        free(fileContents);
      }

      if(current.type == SavableType::Editor)
      {
        cJSON_AddItemToObject(root, object->string, object);
      }
      else if(current.type == SavableType::Model)
      {
        cJSON_AddItemToArray(array, object);
      }
    }

    const char *rendered = cJSON_Print(root);
    cJSON_Delete(root);

    mtar_write_file_header(&tar, "scene.json", strlen(rendered));
    mtar_write_data(&tar, rendered, strlen(rendered));

    mtar_finalize(&tar);
    mtar_close(&tar);

    return Compress(saveName);
  }
  
  return false;
}

bool CFileIO::Load(cJSON **data, string &fileName)
{
  OPENFILENAME ofn;
  char szFile[260];
  string rootPath = RootPath();
  
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "UltraEd (*.ultra)";
  ofn.nFilterIndex = 1;
  ofn.lpstrTitle = "Load Scene";
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  
  if(GetOpenFileName(&ofn) && Decompress(&ofn.lpstrFile))
  {
    fileName = CleanFileName(ofn.lpstrFile);

    mtar_t tar;
    mtar_header_t header;

    mtar_open(&tar, ofn.lpstrFile, "r");
    mtar_find(&tar, "scene.json", &header);

    char *contents = (char*)calloc(1, header.size + 1);
    mtar_read_data(&tar, contents, header.size);
    cJSON *root = cJSON_Parse(contents);

    // Iterate through all models.
    cJSON *models = cJSON_GetObjectItem(root, "models");
    cJSON *model = NULL;
    cJSON_ArrayForEach(model, models)
    {
      // Locate each packed model resource.
      cJSON *resources = cJSON_GetObjectItem(model, "resources");
      cJSON *resource = NULL;
      cJSON_ArrayForEach(resource, resources)
      {
        char target[128];
        const char *fileName = resource->child->valuestring;

        // Locate the resource to extract.
        mtar_find(&tar, fileName, &header);
        char *buffer = (char*)calloc(1, header.size + 1);
        mtar_read_data(&tar, buffer, header.size);

        // Format path and write to library.
        sprintf(target, "%s\\%s", rootPath.c_str(), fileName);
        FILE *file = fopen(target, "wb");
        fwrite(buffer, 1, header.size, file);
        fclose(file);
        free(buffer);

        // Update the path to the fully qualified target.
        resource->child->valuestring = strdup(target);
      }
    }

    mtar_close(&tar);
    free(contents);
    remove(ofn.lpstrFile);

    // Pass the constructed json object out.
    *data = root;

    return true;
  }

  return false;
}

FileInfo CFileIO::Import(const char *file)
{
  FileInfo info;
  string rootPath = RootPath();

  // When a GUID then must have already been imported so don't re-import.
  if(CUtil::StringToGuid(PathFindFileName(file)) != GUID_NULL)
  {
    info.path = file;
    info.type = User;
    return info;
  }

  if(CreateDirectory(rootPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
  {
    char target[MAX_PATH];

    const char *assets = "Assets/";
    if(strncmp(file, assets, strlen(assets)) == 0)
    {
      info.path = file;
      info.type = Editor;
      return info;
    }

    // Format new imported path.
    sprintf(target, "%s\\%s", rootPath.c_str(), CUtil::GuidToString(CUtil::NewGuid()).c_str());

    if(CopyFile(file, target, FALSE))
    {
      info.path = target;
      info.type = User;
      return info;
    }
  }

  info.path = file;
  info.type = Unknown;
  return info;
}

bool CFileIO::Compress(const char *path)
{
  FILE *file = fopen(path, "rb");
  if(file == NULL) return false;

  // Get the total size of the file.
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);

  // Read in entire file.
  char *data = (char*)malloc(size);
  if(data == NULL) return false;
  int bytesRead = fread(data, 1, size, file);
  if(bytesRead != size) return false;
  fclose(file);

  // Compressed buffer must be at least 5% larger.
  char *compressed = (char*)malloc(size + (size * 0.05));
  if(compressed == NULL) return false;
  int bytesCompressed = fastlz_compress(data, size, compressed);
  if(bytesCompressed == 0) return false;

  // Annotate compressed data with uncompressed size.
  int annotatedSize = bytesCompressed + sizeof(int);
  char *buffer = (char*)malloc(annotatedSize);
  memcpy(buffer, &size, sizeof(int));
  memcpy(buffer + sizeof(int), compressed, bytesCompressed);

  // Write compressed file back out.
  file = fopen(path, "wb");
  if(file == NULL) return false;
  unsigned int bytesWritten = fwrite(buffer, 1, annotatedSize, file);
  if(bytesWritten != annotatedSize) return false;
  fclose(file);

  free(compressed);
  free(buffer);
  free(data);

  return true;
}

bool CFileIO::Decompress(char **path)
{
  FILE *file = fopen(*path, "rb");
  if(file == NULL) return false;

  // Get the total size of the file.
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);

  // Read in entire file.
  char *data = (char*)malloc(size);
  if(data == NULL) return false;
  int bytesRead = fread(data, 1, size, file);
  if(bytesRead != size) return false;
  fclose(file);

  // Read the uncompressed file length.
  int uncompressedSize = 0;
  memmove(&uncompressedSize, data, sizeof(int));
  memmove(data, data + sizeof(int), size - sizeof(int));

  char *decompressed = (char*)malloc(uncompressedSize);
  if(decompressed == NULL) return false;
  int bytesDecompressed = fastlz_decompress(data, size - sizeof(int), decompressed, uncompressedSize);
  if(bytesDecompressed == 0) return false;

  // Create a temp path to extract the scene file.
  string pathBuffer(*path);
  string tempName(tmpnam(NULL));
  pathBuffer.append(tempName.erase(0,1));

  // Write decompressed file back out.
  file = fopen(pathBuffer.c_str(), "wb");
  if(file == NULL) return false;
  unsigned int bytesWritten = fwrite(decompressed, 1, bytesDecompressed, file);
  if(bytesWritten != bytesDecompressed) return false;
  fclose(file);
  free(decompressed);
  free(data);

  // Pass the new path out.
  *path = strdup(pathBuffer.c_str());

  return true;
}

string CFileIO::RootPath()
{
    char pathBuffer[MAX_PATH];
    GetModuleFileName(NULL, pathBuffer, MAX_PATH);
    string pathString(pathBuffer);
    pathString = pathString.substr(0, pathString.find_last_of("\\/"));
    pathString.append("\\Library");
    return pathString;
}

string CFileIO::CleanFileName(const char *fileName)
{
  string cleanedName(PathFindFileName(fileName));
  string::size_type pos = cleanedName.find('.');
  if(pos != string::npos) cleanedName.erase(pos, string::npos);
  return cleanedName;
}
