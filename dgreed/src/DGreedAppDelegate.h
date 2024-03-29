#import <UIKit/UIKit.h>
#import <StoreKit/StoreKit.h>
#import "GLESView.h"
#import "GLESViewController.h"
#import "AutoRotateViewController.h"
#import <MessageUI/MFMailComposeViewController.h>

@interface DGreedAppDelegate : NSObject <
UIApplicationDelegate, 
GKLeaderboardViewControllerDelegate, 
GKAchievementViewControllerDelegate,
UIAccelerometerDelegate,
SKProductsRequestDelegate,
SKPaymentTransactionObserver,
UIImagePickerControllerDelegate,
UIPopoverControllerDelegate,
UINavigationControllerDelegate,
MFMailComposeViewControllerDelegate
> {
    
    UIWindow* window;
    AutoRotateViewController* controller;
    GLESViewController* gl_controller;
	GLESView* gl_view;
    UIAcceleration* last_acceleration;
    BOOL is_shaking;
}

@property(nonatomic,retain) UIWindow* window;
@property(nonatomic,retain) AutoRotateViewController* controller;
@property(nonatomic,retain) GLESViewController* gl_controller;
@property(nonatomic,retain) UIAcceleration* last_acceleration;
@property(nonatomic,assign) BOOL is_ipad;

@end

