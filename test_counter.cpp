// Copyright (C) Intel Corp.  2018.  All Rights Reserved.

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice (including the
// next paragraph) shall be included in all copies or substantial
// portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//  **********************************************************************/
//  * Authors:
//  *   Mark Janes <mark.a.janes@intel.com>
//  **********************************************************************/

#include <assert.h>
#include <epoxy/gl.h>
#include <stdio.h>
#include <string.h>
#include <waffle-1/waffle.h>

#include <map>
#include <vector>

int main() {
  const int32_t init_attrs[] = {
    WAFFLE_PLATFORM, WAFFLE_PLATFORM_GLX,
    0,
  };
  waffle_init(init_attrs);

  struct waffle_display *dpy = waffle_display_connect(NULL);

  const int32_t config_attrs[] = {
    WAFFLE_CONTEXT_API, WAFFLE_CONTEXT_OPENGL,
    0,
  };

  struct waffle_config *config = waffle_config_choose(dpy, config_attrs);
  struct waffle_window *window = waffle_window_create(config, 1000, 1000);
  struct waffle_context *ctx = waffle_context_create(config, NULL);
  waffle_make_current(dpy, window, ctx);

  waffle_window_show(window);

  GLint count;
  bool has_metrics = false;
  glGetIntegerv(GL_NUM_EXTENSIONS, &count);
  for (int i = 0; i < count; ++i) {
    const GLubyte *name = glGetStringi(GL_EXTENSIONS, i);
    if (strcmp((const char*)name, "GL_AMD_performance_monitor") == 0)
      has_metrics = true;
  }
  if (!has_metrics) {
    printf("ERROR: unsupported platform\n");
    return -1;
  }
  
  int num_groups;
  glGetPerfMonitorGroupsAMD(&num_groups, 0, NULL);
  std::vector<uint> groups(num_groups);
  glGetPerfMonitorGroupsAMD(&num_groups, num_groups, groups.data());
  GLuint target_group = 0, target_counter = 0;
  for (auto group : groups) {
    GLint max_name_len = 0;
    glGetPerfMonitorGroupStringAMD(group, 0,
                                   &max_name_len, NULL);
    std::vector<GLchar> group_name(max_name_len + 1);
    GLsizei name_len;
    glGetPerfMonitorGroupStringAMD(group, max_name_len + 1,
                                   &name_len, group_name.data());
    int num_counters, max_active_counters;
    glGetPerfMonitorCountersAMD(group,
                                &num_counters,
                                &max_active_counters,
                                0, NULL);
    std::vector<uint> counters(num_counters);
    glGetPerfMonitorCountersAMD(group,
                                &num_counters,
                                &max_active_counters,
                                num_counters, counters.data());
    for (auto counter : counters) {
      GLsizei length;
      glGetPerfMonitorCounterStringAMD(
          group, counter, 0, &length, NULL);
      std::vector<char> counter_name(length + 1);
      glGetPerfMonitorCounterStringAMD(group, counter, length + 1,
                                       &length, counter_name.data());
      GLuint counter_type;
      glGetPerfMonitorCounterInfoAMD(group, counter,
                                     GL_COUNTER_TYPE_AMD, &counter_type);
      std::map<GLuint, std::string> type_map {
        {GL_UNSIGNED_INT64_AMD, "GL_UNSIGNED_INT64_AMD"},
        {GL_PERCENTAGE_AMD, "GL_PERCENTAGE_AMD"},
        {GL_UNSIGNED_INT, "GL_UNSIGNED_INT"},
        {GL_FLOAT, "GL_FLOAT"}
      };
      printf("group:%s(%d)\tcounter:%s(%d)\ttype:(%s)\n",
             group_name.data(), group,
             counter_name.data(), counter,
             type_map[counter_type].c_str());
      if (strcmp("GRBM_000", counter_name.data()) == 0) {
        target_counter = counter;
        target_group = group;
      }
    }
  }

  std::vector<unsigned int> monitors(100);
  glGenPerfMonitorsAMD(monitors.size(), monitors.data());
  for (auto monitor : monitors)
    glSelectPerfMonitorCountersAMD(monitor, GL_TRUE, target_group, 1,
                                   &target_counter);
  std::vector<unsigned int> used_monitors;
  std::vector<unsigned char> buffer;
  while(true) {
    if (monitors.empty()) {
      glFinish();
      GLuint resultSize;
      for (auto monitor : used_monitors) {
        glGetPerfMonitorCounterDataAMD(monitor, GL_PERFMON_RESULT_SIZE_AMD, 
                                       sizeof(GLint), &resultSize, NULL);
        buffer.resize(resultSize);
        GLsizei bytesWritten;
        glGetPerfMonitorCounterDataAMD(monitor, GL_PERFMON_RESULT_AMD,  
                                       resultSize,
                                       (uint*) buffer.data(), &bytesWritten);
        if (bytesWritten == 0)
          printf("WARN: no data from counter\n");
        else {
          GLuint group  = *((GLuint*) buffer.data());
          GLuint counter = *((GLuint*) buffer.data() + sizeof(GLuint));
          uint64_t counterResult = *(uint64_t*)(buffer.data() + 2*sizeof(GLuint));
          printf("clocks: %lld\n", counterResult);
        }
        monitors.push_back(monitor);
      }
      used_monitors.clear();
    }
    auto monitor = monitors.back();
    monitors.pop_back();
    used_monitors.push_back(monitor);
    glBeginPerfMonitorAMD(monitor);
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEndPerfMonitorAMD(monitor);
    waffle_window_swap_buffers(window);
    glClearColor(0.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    waffle_window_swap_buffers(window);
  }

  return 0;
}
