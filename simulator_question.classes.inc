<?php
/**
 * Extension of QuizQuestion.
 */
class SimulatorQuestion extends QuizQuestion {
  /**
   * Implementation of saveNodeProperties
   */
  public function saveNodeProperties($is_new = FALSE) {
    if (!isset($this->node->feedback)) {
      $this->node->feedback = '';
    }
    $this->node->device_metrics = json_encode($this->node->device_metrics);
    $this->node->device_config = json_encode($this->node->device_config);
    if ($is_new || $this->node->revision == 1) {
      $id = db_insert('quiz_simulator_question_node_properties')
        ->fields(array(
          'nid' => $this->node->nid,
          'vid' => $this->node->vid,
          'device_config' => $this->node->device_config,
          'device_id' => $this->node->device_select,
          'device_default' => json_encode($this->node->device),
          'device_metrics' => $this->node->device_metrics,
          'required' => $this->node->required,
        ))
        ->execute();
    }
    else {
      db_update('quiz_simulator_question_node_properties')
        ->fields(array(
          'device_config' => $this->node->device_config,
          'device_default' => json_encode($this->node->device),
          'device_metrics' => $this->node->device_metrics,
          'required' => $this->node->required,
        ))
        ->condition('nid', $this->node->nid)
        ->condition('vid', $this->node->vid)
        ->execute();
    }
  }

  /**
   * Implementation of validateNode
   */
  public function validateNode(array &$form) {
  }

  /**
   * Implementation of delete
   */
  public function delete($only_this_version = FALSE) {
    if ($only_this_version) {
      db_delete('quiz_simulator_question_user_answers')
        ->condition('question_nid', $this->node->nid)
        ->condition('question_vid', $this->node->vid)
        ->execute();
      db_delete('quiz_simulator_question_node_properties')
        ->condition('nid', $this->node->nid)
        ->condition('vid', $this->node->vid)
        ->execute();
    }
    else {
      db_delete('quiz_simulator_question_node_properties')
        ->condition('nid', $this->node->nid)
        ->execute();
      db_delete('quiz_simulator_question_user_answers')
        ->condition('question_nid', $this->node->nid)
        ->execute();
    }
    parent::delete($only_this_version);
  }

  /**
   * Implementation of getNodeProperties
   */
  public function getNodeProperties() {
    if (isset($this->nodeProperties)) {
      return $this->nodeProperties;
    }
    $props = parent::getNodeProperties();

    $res_a = db_query('SELECT device_id, device_vid, device_default, device_config, device_metrics, required FROM {quiz_simulator_question_node_properties}
      WHERE nid = :nid AND vid = :vid', array(':nid' => $this->node->nid, ':vid' => $this->node->vid))->fetchAssoc();

    if (is_array($res_a)) {
      $props = array_merge($props, $res_a);
    }
    $props['device_default'] = json_decode($props['device_default']);
    $props['device_config'] = json_decode($props['device_config']);
    $props['device_metrics'] = json_decode($props['device_metrics']);

    $this->nodeProperties = $props;
    return $props;
  }

  /**
   * Implementation of getNodeView
   */
  public function getNodeView() {
    $content = parent::getNodeView();
    /*
    if ($this->viewCanRevealCorrect()) {
      $content['answers'] = array(
        '#type' => 'item',
        '#title' => t('Rubric'),
        '#markup' => '<div class="quiz-solution">' . check_markup($this->node->rubric, $this->getFormat()) . '</div>',
        '#weight' => 1,
      );
    }
    else {
      $content['answers'] = array(
        '#markup' => '<div class="quiz-answer-hidden">Answer hidden</div>',
        '#weight' => 1,
      );
    }
    */
    return $content;
  }

  /**
   * Implementation of getAnweringForm
   */
  public function getAnsweringForm(array $form_state = NULL, $rid) {
    $response = new SimulatorResponse($rid, $this->node);
    return;
  }

  /**
   * Implementation of getCreationForm
   */
  public function getCreationForm(array &$form_state = NULL) {

    if (isset($form_state['node']) && $node = $form_state['node']) {
      if (isset($node->device_default)) {
        $device = $node->device_default;
        $disabled = TRUE;
      }
      else {
        $disabled = FALSE;
      }
    }

    // If we have a pre-selected device, load it
    if (isset($node->device_id)) {
      $device = xlms_device_load($node->device_id);
      simulator_question_load_device($device);
    }

    // Load all available devices
    $devices = xlms_device_load_all();

    // Build select list of devices for question creation page
    $options = array(0 => t('Choose one'));
    foreach ($devices as $d) {
      $options[$d->did] = $d->label;
    }

    $form['device_select'] = array(
      '#type' => 'select',
      '#title' => t('Trainer'),
      '#description' => $disabled ? NULL : t('Select an available trainer for this exercise or ') . l(t('add another.'), 'admin/config/xlms/devices/add') ,
      //'#options' => array(0 => 'Choose one', 'airway' => t('Airway'), 'orthobox' => t('Orthobox')),
      '#options' => $options,
      '#disabled' => $disabled,
      '#ajax' => array(
        'callback' => 'simulator_question_metrics_form_callback',
        'wrapper' => 'device',
        'method' => 'replace',
        'effect' => 'fade',
      ),
      '#default_value' => isset($device->did) ? $device->did : 0,
    );
    $form['#prefix'] = '<div id="device">';
    $form['#suffix'] = '</div>';

    $form['device_metrics'] = array(
       '#type' => 'hidden',
       '#title' => t('Metrics'),
       '#collapisible' => FALSE,
       '#collapsed' => FALSE,
       '#tree' => TRUE,
    );
    $form['device_config'] = array(
      '#type' => 'hidden',
    );

    if (isset($device) || !empty($form_state['values']['device_select'])) {
      $form['device_metrics']['#type'] = 'fieldset';

      // Reset input for metrics during AJAX callback so default values can be set correctly.
      unset($form_state['input']['device_metrics']);

      if (!empty($form_state['values']['device_select'])) {
        unset($device);
        $device = $devices[$form_state['values']['device_select']];
        simulator_question_load_device($device);
        if ($node->device_default->name == $form_state['values']['device_select']) {
          $metric_values = $node->device_metrics;
          $configuration_values = $node->device_config;
        }
      }
      elseif (is_object($device) && ($device->did == $node->device_default->did)) {
        $metric_values = $node->device_metrics;
        $configuration_values = $node->device_config;
      }

      $form_state['storage']['device'] = $device;
      foreach ($device->info->metrics as $metric => $data) {
        $form['device_metrics'][$metric] = array(
          "#type" => 'fieldset',
          "#title" => t($data->description),
        );
        /*
        foreach ($data->values as $key => $data) {
          $form['device_metrics'][$metric][$key]['unit'] = array(
            "#type" => 'hidden',
            '#name' => 'unit',
            '#value' => $data->unit,
          );
          $form['device_metrics'][$metric][$key]['threshold'] = array(
            "#type" => 'hidden',
            '#name' => 'threshold',
            '#value' => $data->threshold,
          );
          $form['device_metrics'][$metric][$key]['default'] = array(
            "#type" => 'textfield',
            "#parent" => 'device_metrics',
            "#title" => t($data->threshold),
            "#default_value" => (isset($metric_values)) ? $metric_values->{$metric}[$key]->default: $data->default,
          );
        }
        */
        foreach ($data->values as $value) {
          $form['device_metrics'][$metric][$value->threshold] = array(
            "#type" => 'textfield',
            "#parent" => 'device_metrics',
            "#title" => t($value->threshold),
            "#default_value" => (isset($metric_values)) ? $metric_values->{$metric}->{$value->threshold} : $value->default,
          );
        }
      }
      $form['device_config'] = array(
        '#type' => 'fieldset',
        '#title' => t('Device configuration'),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
      foreach ($device->info->configuration as $configuration => $data) {
        if (is_array($data->default)) {
          $form['device_config'][$configuration] = array(
            "#type" => 'fieldset',
            '#title' => t($data->description),
            '#collapisible' => FALSE,
            '#collapsed' => FALSE,
            '#tree' => TRUE,
            '#prefix' => '<div id="' . drupal_clean_css_identifier($configuration) .'">',
            '#suffix' => '</div>',
          );
          $form['device_config'][$configuration]['add_option'] = array(
            '#type' => 'button',
            '#value' => t('Add another'),
            '#name' => $configuration,
            '#ajax' => array(
              'callback' => 'simulator_question_config_multi_value_callback',
              'wrapper' => drupal_clean_css_identifier($configuration),
              'method' => 'replace',
            ),
            '#parents' => array('device_config'),
          );
          if (empty($form_state['storage'][$configuration])) {
            if (empty($configuration_values)) {
              $form_state['storage'][$configuration] = $device->info->configuration->{$configuration}->default;
            }
            else {
              $form_state['storage'][$configuration] = $configuration_values->$configuration;
            }
          }
          if ($form_state['triggering_element']['#name'] == $configuration) {
            $count = count($form_state['storage'][$configuration]);
            $form_state['storage'][$configuration][$count] = '';
          }
          if (isset($form_state['storage'][$configuration])) {
            foreach ($form_state['storage'][$configuration] as $key => $value) {
              $form['device_config'][$configuration][$key] = array(
                '#type' => 'textfield',
                '#default_value' => $value,
              );
            }
          }

        }
        else {
          $form['device_config'][$configuration] = array(
            "#type" => 'textfield',
            "#title" => t($data->description),
            "#default_value" => (isset($configuration_values)) ? $configuration_values->{$configuration} : $data->default,
          );
        }
      }
    }

    $form['#tree'] = TRUE;
    return $form;
  }

  /**
   * Implementation of getMaximumScore
   */
  public function getMaximumScore() {
    return variable_get('simulator_question_default_max_score', 10);
  }

  public function getSimulationConfig(&$session) {
    $config = array();
    $device = xlms_device_load($this->node->device_id);
    simulator_question_load_device($device);
    $session->name = $device->info->name;
    $session->interface = $device->info->interface;
    $session->hardware = $device->info->hardware;
    $session->metrics = $this->node->device_metrics;
    $session->configuration = $this->node->device_config;
    //$config['config'] = $this->node->device_metric;
    //$config['threshold'] = $this->node->threshold;
    // Probably need another parameter for how to evaluate threshold.
    //return $config;
  }

}

/**
 * Extension of QuizQuestionResponse
 */
class SimulatorResponse extends QuizQuestionResponse {

  /**
   * ID of the answer.
   */
  protected $answer_id = 0;
  protected $is_correct = TRUE;
  protected $evaluated = FALSE;
  protected $answer = NULL;
  protected $rid = 0;

  // The question node(not a quiz question instance)
  public $question = NULL;

  protected $score = 0;
  public $is_skipped;
  public $is_doubtful;

  /**
   * Constructor
   */
  public function __construct($result_id, stdClass $question_node, $answer = NULL) {
    parent::__construct($result_id, $question_node, $answer);

    if (!isset($answer)) {
      // Question has been answered allready. We fetch the answer data from the database.
      $r = db_query('SELECT answer_id, answer, is_evaluated, score, question_vid, question_nid, result_id, answer_feedback, answer_feedback_format
        FROM {quiz_simulator_question_user_answers}
        WHERE question_nid = :qnid AND question_vid = :qvid AND result_id = :rid', array(':qnid' => $question_node->nid, ':qvid' => $question_node->vid, ':rid' => $result_id))->fetch();

      if (!empty($r)) {
        $this->answer = $r->answer;
        $this->score = $r->score;
        $this->evaluated = $r->is_evaluated;
        $this->answer_id = $r->answer_id;
        $this->answer_feedback = $r->answer_feedback;
        $this->answer_feedback_format = $r->answer_feedback_format;
      }
    }
    else {
      $this->answer = $answer;
      $this->evaluated = FALSE;
    }
  }

  /**
   * Implementation of isValid
   */
  public function isValid() {
    return TRUE;
  }

  /**
   * Implementation of save
   */
  public function save() {
    $this->answer_id = db_insert('quiz_simulator_question_user_answers')
      ->fields(array(
        'answer' => 0,
        'question_nid' => $this->question->nid,
        'question_vid' => $this->question->vid,
        'result_id' => $this->rid,
      ))
      ->execute();
  }

  /**
   * Implementation of delete
   */
  public function delete() {
    db_delete('quiz_simulator_question_user_answers')
      ->condition('question_nid', $this->question->nid)
      ->condition('question_vid', $this->question->vid)
      ->condition('result_id', $this->rid)
      ->execute();
  }

  /**
   * Implementation of score
   */
  public function score() {
    return (int) db_query('SELECT score FROM {quiz_simulator_question_user_answers}
      WHERE result_id = :result_id AND question_vid = :question_vid', array(':result_id' => $this->rid, ':question_vid' => $this->question->vid))->fetchField();
  }

  /**
   * Implementation of getResponse
   */
  public function getResponse() {
    return $this->answer;
  }

  /**
   * Implementation of getReportFormResponse
   */
  public function getReportFormResponse($showpoints = TRUE, $showfeedback = TRUE, $allow_scoring = FALSE) {
    $form = array();
    $form['#theme'] = 'simulator_question_response_form';
    /*
    if ($this->question && !empty($this->question->answers)) {
      $answer = (object) current($this->question->answers);
    }
    else {
      return $form;
    }
    //$form['answer'] = array('#markup' => check_markup($answer->answer));
    if ($answer->is_evaluated == 1) {
      // Show feedback, if any.
      if ($showfeedback && !empty($answer->feedback)) {
        //$feedback = check_markup($answer->feedback);
      }
    }
    else {
      $feedback = t('This answer has not yet been scored.') .
        '<br/>' .
        t('Until the answer is scored, the total score will not be correct.');
    }
    */
    $form['metric'] = array(
      '#title' => t('Metric evaluated'),
      '#title_display' => 'invisible',
      '#type' => 'item',
      '#markup' => t('Metric') . ': <span>' . $this->question->metric . '</span>',
    );
    $form['threshold'] = array(
      '#title' => t('Threshold used for evaluation'),
      '#title_display' => 'invisible',
      '#type' => 'item',
      '#markup' => t('Threshold used for evaluation') . ': <span>' . $this->question->threshold . '</span>',
    );
/*
    $form['required'] = array(
      '#title' => t('Is this question required to pass'),
      '#type' => 'item',
      '#markup' => $this->question->required,
    );

    if ($allow_scoring) {

    }
    if (!$allow_scoring && !empty($this->answer_feedback)) {
      $form['answer_feedback'] = array(
        '#title' => t('Feedback'),
        '#type' => 'item',
        '#markup' => '<span class="quiz_answer_feedback">' . check_markup($this->answer_feedback, $this->answer_feedback_format) . '</span>',
      );
    }
    if (!empty($feedback)) {
      $form['feedback'] = array('#markup' => '<span class="quiz_answer_feedback">' . $feedback . '</span>');
    }
*/
    return $form;
  }

  /**
   * Implementation of getReportFormScore
  public function getReportFormScore($showfeedback = TRUE, $showpoints = TRUE, $allow_scoring = FALSE) {
    // The score will be shown as a questionmark if the question haven't been scored already
    $score = ($this->isEvaluated()) ? $this->getScore() : 0;
    return array('#markup' => $score);
  }
   */

  /*
    public function getReportFormAnswerFeedback($showpoints = TRUE, $showfeedback = TRUE, $allow_scoring = FALSE) {
      if (quiz_access_to_score() && $allow_scoring) {
        return array(
          '#title' => t('Feedback'),
          '#type' => 'textarea',
          '#rows' => 2,
          '#default_value' => isset($this->answer_feedback) ? check_markup($this->answer_feedback, $this->answer_feedback_format) : '',
          '#format' => isset($this->answer_feedback_format) ? $this->answer_feedback_format : NULL,
          '#attributes' => array('class' => array('quiz-report-score')),
        );
      }
    return FALSE;
  }
  */


  /**
   * Implementation of getReportFormSubmit
  public function getReportFormSubmit($showfeedback = TRUE, $showpoints = TRUE, $allow_scoring = FALSE) {
    return $allow_scoring ? 'simulator_question_report_submit' : FALSE;
  }
   */

  /**
   * Implementation of getReportFormValidate
  public function getReportFormValidate($showfeedback = TRUE, $showpoints = TRUE, $allow_scoring = FALSE) {
    //return $allow_scoring ? 'simulator_question_report_validate' : FALSE;
  }
   */

  /**
   * Check to see if the answer is marked as correct.
  function getScore($weight_adjusted = TRUE) {
    if ($this->is_skipped) {
      return 0;
    }
    if (!isset($this->score)) {
      $this->score = $this->score();
    }
    if (isset($this->question->score_weight) && $weight_adjusted) {
      return round($this->score * $this->question->score_weight);
    }
    return $this->score;
  }
   */
}
