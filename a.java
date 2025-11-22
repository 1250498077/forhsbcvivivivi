import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import java.util.*;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;

public class JiraStoryCreator {
    
    private HttpClient httpClient;
    private String jiraBaseUrl = "https://wpb-jira.systems.uk.hsbc";
    private Gson gson = new Gson();
    
    public JiraStoryCreator(HttpClient httpClient) {
        this.httpClient = httpClient;
    }
    
    /**
     * 获取项目创建 issue 时可用的自定义字段
     */
    public Set<String> getCreatableCustomFields(String projectKey, String issueType) {
        try {
            String encodedIssueType = URLEncoder.encode(issueType, StandardCharsets.UTF_8);
            String metaUrl = String.format("%s/rest/api/2/issue/createmeta?projectKeys=%s&issuetypeNames=%s&expand=projects.issuetypes.fields",
                    jiraBaseUrl, projectKey, encodedIssueType);
            
            HttpResponse response = httpClient.httpRequest(metaUrl, "GET");
            JsonObject meta = JsonParser.parseString(response.getText()).getAsJsonObject();
            
            // 导航到字段定义
            JsonArray projects = meta.getAsJsonArray("projects");
            if (projects.size() == 0) {
                return new HashSet<>();
            }
            
            JsonObject project = projects.get(0).getAsJsonObject();
            JsonArray issuetypes = project.getAsJsonArray("issuetypes");
            if (issuetypes.size() == 0) {
                return new HashSet<>();
            }
            
            JsonObject fields = issuetypes.get(0).getAsJsonObject().getAsJsonObject("fields");
            
            // 提取所有 customfield 字段
            Set<String> creatableFields = new HashSet<>();
            for (String fieldKey : fields.keySet()) {
                if (fieldKey.startsWith("customfield_")) {
                    JsonObject fieldMeta = fields.getAsJsonObject(fieldKey);
                    // 检查字段是否必需或允许设置
                    if (!fieldMeta.has("operations") || 
                        fieldMeta.getAsJsonArray("operations").size() > 0) {
                        creatableFields.add(fieldKey);
                    }
                }
            }
            
            return creatableFields;
            
        } catch (Exception e) {
            System.err.println("Error getting creatable fields: " + e.getMessage());
            return new HashSet<>();
        }
    }
    
    /**
     * 从模板 story 创建新的 story
     */
    public String createStoryFromTemplate(String templateStoryId, String newSummary) {
        try {
            // 1. 获取模板 story 的完整信息
            String getUrl = String.format("%s/rest/api/2/issue/%s", jiraBaseUrl, templateStoryId);
            HttpResponse templateResponse = httpClient.httpRequest(getUrl, "GET");
            JsonObject templateIssue = JsonParser.parseString(templateResponse.getText()).getAsJsonObject();
            JsonObject templateFields = templateIssue.getAsJsonObject("fields");
            
            // 2. 获取项目和 issue 类型
            String projectKey = templateFields.getAsJsonObject("project").get("key").getAsString();
            String issueTypeName = templateFields.getAsJsonObject("issuetype").get("name").getAsString();
            
            // 3. 获取可创建的自定义字段列表
            Set<String> creatableFields = getCreatableCustomFields(projectKey, issueTypeName);
            
            // 4. 构建新 story 的字段
            Map<String, Object> newFields = new HashMap<>();
            
            // 必需的基本字段
            Map<String, String> project = new HashMap<>();
            project.put("key", projectKey);
            newFields.put("project", project);
            
            Map<String, String> issuetype = new HashMap<>();
            issuetype.put("name", issueTypeName);
            newFields.put("issuetype", issuetype);
            
            newFields.put("summary", newSummary);
            
            // 可选的基本字段
            if (templateFields.has("description") && !templateFields.get("description").isJsonNull()) {
                newFields.put("description", templateFields.get("description").getAsString());
            }
            
            if (templateFields.has("priority") && !templateFields.get("priority").isJsonNull()) {
                Map<String, String> priority = new HashMap<>();
                priority.put("name", templateFields.getAsJsonObject("priority").get("name").getAsString());
                newFields.put("priority", priority);
            }
            
            if (templateFields.has("assignee") && !templateFields.get("assignee").isJsonNull()) {
                Map<String, String> assignee = new HashMap<>();
                assignee.put("name", templateFields.getAsJsonObject("assignee").get("name").getAsString());
                newFields.put("assignee", assignee);
            }
            
            if (templateFields.has("labels") && !templateFields.get("labels").isJsonNull()) {
                JsonArray labelsArray = templateFields.getAsJsonArray("labels");
                List<String> labels = new ArrayList<>();
                for (JsonElement label : labelsArray) {
                    labels.add(label.getAsString());
                }
                if (!labels.isEmpty()) {
                    newFields.put("labels", labels);
                }
            }
            
            if (templateFields.has("components") && !templateFields.get("components").isJsonNull()) {
                JsonArray componentsArray = templateFields.getAsJsonArray("components");
                List<Map<String, String>> components = new ArrayList<>();
                for (JsonElement comp : componentsArray) {
                    Map<String, String> component = new HashMap<>();
                    component.put("name", comp.getAsJsonObject().get("name").getAsString());
                    components.add(component);
                }
                if (!components.isEmpty()) {
                    newFields.put("components", components);
                }
            }
            
            // 5. 复制允许创建的自定义字段
            for (String fieldKey : creatableFields) {
                if (templateFields.has(fieldKey) && !templateFields.get(fieldKey).isJsonNull()) {
                    JsonElement fieldValue = templateFields.get(fieldKey);
                    
                    // 直接复制字段值（Gson 会处理类型转换）
                    if (fieldValue.isJsonObject()) {
                        newFields.put(fieldKey, gson.fromJson(fieldValue, Map.class));
                    } else if (fieldValue.isJsonArray()) {
                        newFields.put(fieldKey, gson.fromJson(fieldValue, List.class));
                    } else if (fieldValue.isJsonPrimitive()) {
                        newFields.put(fieldKey, fieldValue.getAsString());
                    }
                }
            }
            
            // 6. 创建新 story
            Map<String, Object> payload = new HashMap<>();
            payload.put("fields", newFields);
            
            String createUrl = String.format("%s/rest/api/2/issue/", jiraBaseUrl);
            String requestBody = gson.toJson(payload);
            
            HttpResponse createResponse = httpClient.httpRequest(createUrl, "POST", requestBody);
            JsonObject createdIssue = JsonParser.parseString(createResponse.getText()).getAsJsonObject();
            
            String newIssueKey = createdIssue.get("key").getAsString();
            System.out.println("Successfully created story: " + newIssueKey);
            
            return newIssueKey;
            
        } catch (Exception e) {
            System.err.println("Error creating story from template: " + e.getMessage());
            e.printStackTrace();
            throw new RuntimeException("Failed to create story", e);
        }
    }
    
    /**
     * 批量从模板创建多个 story
     */
    public List<String> createBulkStoriesFromTemplate(String templateStoryId, List<String> summaries) {
        List<String> createdStories = new ArrayList<>();
        
        for (String summary : summaries) {
            try {
                String newStoryKey = createStoryFromTemplate(templateStoryId, summary);
                createdStories.add(newStoryKey);
                
                // 避免请求过快
                Thread.sleep(500);
                
            } catch (Exception e) {
                System.err.println("Failed to create story with summary: " + summary);
                System.err.println("Error: " + e.getMessage());
            }
        }
        
        return createdStories;
    }
    
    /**
     * 获取模板 story 的字段信息（用于预览）
     */
    public Map<String, Object> getTemplateInfo(String storyId) {
        try {
            String getUrl = String.format("%s/rest/api/2/issue/%s", jiraBaseUrl, storyId);
            HttpResponse response = httpClient.httpRequest(getUrl, "GET");
            JsonObject issue = JsonParser.parseString(response.getText()).getAsJsonObject();
            JsonObject fields = issue.getAsJsonObject("fields");
            
            Map<String, Object> templateInfo = new HashMap<>();
            
            // 基本信息
            templateInfo.put("key", issue.get("key").getAsString());
            templateInfo.put("summary", fields.get("summary").getAsString());
            templateInfo.put("issueType", fields.getAsJsonObject("issuetype").get("name").getAsString());
            templateInfo.put("project", fields.getAsJsonObject("project").get("key").getAsString());
            
            // 描述
            if (fields.has("description") && !fields.get("description").isJsonNull()) {
                templateInfo.put("description", fields.get("description").getAsString());
            }
            
            // 自定义字段
            Map<String, Object> customFields = new HashMap<>();
            for (Map.Entry<String, JsonElement> entry : fields.entrySet()) {
                if (entry.getKey().startsWith("customfield_") && !entry.getValue().isJsonNull()) {
                    customFields.put(entry.getKey(), entry.getValue());
                }
            }
            templateInfo.put("customFields", customFields);
            
            return templateInfo;
            
        } catch (Exception e) {
            System.err.println("Error getting template info: " + e.getMessage());
            throw new RuntimeException("Failed to get template info", e);
        }
    }
}

// HttpClient 和 HttpResponse 接口定义（需要根据你的实现调整）
interface HttpClient {
    HttpResponse httpRequest(String url, String method);
    HttpResponse httpRequest(String url, String method, String body);
}

interface HttpResponse {
    String getText();
    int getStatusCode();
}
