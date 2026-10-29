import os
import requests
import json
from datetime import datetime

class TeamsWorkflowNotifier:
    def __init__(self, workflow_url: str = None):
        self.url = workflow_url or os.getenv("TEAMS_WORKFLOW_URL")
        if not self.url:
            raise ValueError("请提供 Workflow URL 或设置 TEAMS_WORKFLOW_URL 环境变量")
        self.session = requests.Session()
        self.session.headers.update({"Content-Type": "application/json"})
    
    # Adaptive Card 只认命名色,不支持 hex。severity -> (文字色, Container 色带样式)
    STYLES = {
        "info":    ("Accent",    "accent"),
        "warning": ("Warning",   "warning"),
        "error":   ("Attention", "attention"),
        "success": ("Good",      "good"),
    }

    def send_alert(self, title: str, message: str, severity: str = "info") -> bool:
        """发送告警卡片。severity 控制卡片配色(info/warning/error/success)。"""
        text_color, container_style = self.STYLES.get(severity, self.STYLES["info"])

        payload = {
            "type": "message",
            "attachments": [{
                "contentType": "application/vnd.microsoft.card.adaptive",
                "content": {
                    "$schema": "http://adaptivecards.io/schemas/adaptive-card.json",
                    "type": "AdaptiveCard",
                    "version": "1.4",
                    "body": [
                        {
                            # Container.style 给整块上色带/底色,是最接近「卡片颜色」的效果
                            "type": "Container",
                            "style": container_style,
                            "bleed": True,
                            "items": [
                                {
                                    "type": "TextBlock",
                                    "size": "Large",
                                    "weight": "Bolder",
                                    "text": title,
                                    "color": text_color,
                                    "wrap": True
                                },
                                {
                                    "type": "TextBlock",
                                    "text": message,
                                    "wrap": True,
                                    "spacing": "Small"
                                }
                            ]
                        },
                        {
                            "type": "TextBlock",
                            "text": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                            "size": "Small",
                            "isSubtle": True,
                            "spacing": "Small"
                        }
                    ]
                }
            }]
        }
        return self._post(payload)
    
    def _post(self, payload: dict) -> bool:
        try:
            resp = self.session.post(self.url, data=json.dumps(payload), timeout=10)
            resp.raise_for_status()
            return True
        except requests.exceptions.RequestException as e:
            print(f"Teams 发送失败: {e}")
            return False
