#define HOME_TPL \
  "<html><head><title>Sonnen SmartSwitch (Rel: %1$." STRING(_cs(release_tag)) "s)</title></head><body>" \
  "<body><h1>Sonnen SmartSwitch (Rel: %1." STRING(_cs(release_tag)) "$s)</h1>" \
  "<p><form action=\"/api\" method=\"post\">" \
  "<fieldset><legend>Switch</legend>" \
  "<input type=\"radio\" name=\"mode\" value=\"0\" %s><label>Off</label>" \
  "<input type=\"radio\" name=\"mode\" value=\"1\" %s><label>On</label>" \
  "<input type=\"radio\" name=\"mode\" value=\"2\" %s><label>Auto</label>" \
  "<input type=\"submit\" value=\"Send\" />" \
  "</fieldset>" \
  "</form></p>" \
  \
  "<p><form action=\"/api\" method=\"post\">" \
  "<fieldset><legend>Update-Check on startup</legend>" \
  "<input type=\"radio\" name=\"update_startup\" value=\"0\" %s/><label>Off</label>" \
  "<input type=\"radio\" name=\"update_startup\" value=\"1\" %s/><label>On</label>" \
  "<input type=\"submit\" name=\"check\" value=\"Send\" />" \
  "</fieldset>" \
  "</form></p>" \
  \
  "<p><form action=\"api\" method=\"post\" onSubmit=\"return confirm('Sure?');\"><fieldset><legend>Update</legend>" \
  "<input type=\"submit\" name=\"update\" value=\"Update\"/>" \
  "</fieldset>" \
  "</form></p>" \
  \
  "<p><a href=\"/update\">update</a></p>" \
  \
  "<p><form action=\"api\" method=\"post\" onSubmit=\"return confirm('Sure? WLAN Access Data will be reset!');\" ><fieldset><legend>Reset</legend>" \
  "<input type=\"submit\" name=\"reset\" value=\"Reset\"/>" \
  "</fieldset>" \
  "</form></p>" \
  \
  "<p><form action=\"/api\" method=\"post\">" \
  "<fieldset><legend>Sonnen Battery</legend>" \
  "Host: <input name=\"sn_host\" value=\"%s\"><br/>" \
  "Token: <input name=\"sn_token\" value=\"%s\"><br/>" \
  "Grid_min (W): <input name=\"sn_grdmin\" value=\"%d\"><br/>" \
  "Load (W): <input name=\"sn_cpower\" value=\"%d\"><br/>" \
  "<input type=\"submit\" name=\"sonnen\" value=\"Update\" />" \
  "</fieldset>" \
  "</form></p>" \
  \
  "<p><form action=\"/api\" method=\"post\">" \
  "<fieldset><legend>Weather</legend>" \
  "Station Id: (DWD) <input name=\"wt_sid\" value=\"%s\"><br/>" \
  "Longitude: <input name=\"wt_log\" value=\"%f\"><br/>" \
  "Latitude: <input name=\"wt_lat\" value=\"%f\"><br/>" \
  "<input type=\"submit\" name=\"weather\" value=\"Update\" />" \
  "</fieldset>" \
  "</form></p>" \
  "</body></html>"
